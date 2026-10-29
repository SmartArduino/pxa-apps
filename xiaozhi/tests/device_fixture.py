#!/usr/bin/env python3
"""LAN-only hardware fixture; report Opus statistics without saving microphone audio."""
import argparse
import base64
import ctypes
import hashlib
import json
import math
from pathlib import Path
import socketserver
import struct
import threading
import time


def exact(conn, count):
    data = bytearray()
    while len(data) < count:
        chunk = conn.recv(count - len(data))
        if not chunk:
            raise EOFError
        data.extend(chunk)
    return bytes(data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bind', required=True)
    parser.add_argument('--port', type=int, default=8765)
    parser.add_argument('--token-file', required=True, type=Path)
    parser.add_argument('--artifacts', required=True, type=Path)
    parser.add_argument('--frame-samples', type=int, default=320)
    parser.add_argument('--emotions', action='store_true', help='cycle whole-body poses during playback')
    args = parser.parse_args()
    args.artifacts.mkdir(parents=True, exist_ok=True)
    token = args.token_file.read_text().strip()
    assert token
    lib = ctypes.CDLL('libopus.so.0')
    for name in ('encoder', 'decoder'):
        create = getattr(lib, f'opus_{name}_create')
        create.restype = ctypes.c_void_p
        create.argtypes = [ctypes.c_int, ctypes.c_int] + ([ctypes.c_int] if name == 'encoder' else []) + [ctypes.POINTER(ctypes.c_int)]
        getattr(lib, f'opus_{name}_destroy').argtypes = [ctypes.c_void_p]
    lib.opus_encode.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_int16), ctypes.c_int, ctypes.POINTER(ctypes.c_ubyte), ctypes.c_int32]
    lib.opus_decode.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int32, ctypes.POINTER(ctypes.c_int16), ctypes.c_int, ctypes.c_int]
    lib.opus_packet_get_nb_frames.argtypes = [ctypes.c_char_p, ctypes.c_int32]
    lib.opus_packet_get_samples_per_frame.argtypes = [ctypes.c_char_p, ctypes.c_int]
    log_lock = threading.Lock()

    def event(name, **fields):
        with log_lock, (args.artifacts / 'events.jsonl').open('a') as log:
            value = {'event': name, 'time': time.time(), **fields}
            log.write(json.dumps(value, ensure_ascii=False) + '\n')
            print(json.dumps(value, ensure_ascii=False), flush=True)

    class Handler(socketserver.BaseRequestHandler):
        def handle(self):
            conn = self.request
            conn.settimeout(90)
            send_lock = threading.Lock()
            cancelled = threading.Event()
            tts_active = threading.Event()
            error = ctypes.c_int()
            decoder = lib.opus_decoder_create(16000, 1, ctypes.byref(error))
            assert decoder and error.value == 0
            packets = samples = squares = nonzero = replies = 0
            segment = 0
            listening = False
            declared_frame_ms = 0
            worker = None

            def frame(opcode, payload):
                header = bytes([128 | opcode])
                header += bytes([len(payload)]) if len(payload) < 126 else b'\x7e' + struct.pack('!H', len(payload))
                with send_lock:
                    conn.sendall(header + payload)

            def message(value):
                frame(1, json.dumps(value, ensure_ascii=False).encode())

            def reply(number):
                encoder = lib.opus_encoder_create(16000, 1, 2048, ctypes.byref(error))
                assert encoder and error.value == 0
                tts_active.set()
                try:
                    message({'type': 'stt', 'text': '实机链路测试（模拟识别）'})
                    message({'type': 'llm', 'emotion': 'happy'})
                    message({'type': 'tts', 'state': 'start'})
                    message({'type': 'tts', 'state': 'sentence_start', 'text': '麦克风数据已收到，正在播放测试音。'})
                    event('reply_start', number=number)
                    for index in range(100 if number == 1 else 300):
                        if cancelled.is_set():
                            return
                        if args.emotions and index % 25 == 0:
                            mood = ('neutral', 'happy', 'sad', 'thinking', 'surprised',
                                    'angry', 'sleepy', 'neutral')[(index // 25) % 8]
                            message({'type': 'llm', 'emotion': mood})
                            event('emotion', mood=mood, number=number)
                        pcm = (ctypes.c_int16 * 320)(*[int(3000 * math.sin(2 * math.pi * 440 * (index * 320 + i) / 16000)) for i in range(320)])
                        output = (ctypes.c_ubyte * 1275)()
                        count = lib.opus_encode(encoder, pcm, 320, output, 1275)
                        assert count > 0
                        frame(2, bytes(output[:count]))
                        if cancelled.wait(.02):
                            return
                    message({'type': 'tts', 'state': 'stop'})
                    event('reply_stop', number=number)
                except OSError:
                    pass
                finally:
                    tts_active.clear()
                    lib.opus_encoder_destroy(encoder)

            try:
                head = bytearray()
                while not head.endswith(b'\r\n\r\n'):
                    head.extend(exact(conn, 1))
                    assert len(head) <= 16384
                fields = {k.strip().lower(): v.strip() for k, v in (line.split(b':', 1) for line in bytes(head).split(b'\r\n')[1:] if b':' in line)}
                assert fields.get(b'authorization') == ('Bearer ' + token).encode()
                assert fields.get(b'protocol-version') == b'1'
                accept = base64.b64encode(hashlib.sha1(fields[b'sec-websocket-key'] + b'258EAFA5-E914-47DA-95CA-C5AB0DC85B11').digest())
                conn.sendall(b'HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ' + accept + b'\r\n\r\n')
                event('connected')
                while True:
                    first, second = exact(conn, 2)
                    assert first & 128 and second & 128
                    size = second & 127
                    if size == 126:
                        size = struct.unpack('!H', exact(conn, 2))[0]
                    assert size <= 4096 and size != 127
                    mask = exact(conn, 4)
                    data = bytes(v ^ mask[i % 4] for i, v in enumerate(exact(conn, size)))
                    opcode = first & 15
                    if opcode == 8:
                        break
                    if opcode == 2:
                        pcm = (ctypes.c_int16 * 1920)()
                        count = lib.opus_decode(decoder, data, len(data), pcm, 1920, 0)
                        if not packets:
                            event('capture_format', samples=count,
                                  opus_frames=lib.opus_packet_get_nb_frames(data, len(data)),
                                  samples_per_frame=lib.opus_packet_get_samples_per_frame(data, 16000))
                        assert count == args.frame_samples, f'capture frame has {count} samples'
                        assert declared_frame_ms == count // 16, 'hello duration differs from captured packet'
                        packets += 1
                        segment += 1
                        samples += count
                        energy = sum(int(v) ** 2 for v in pcm[:count])
                        squares += energy
                        nonzero += bool(energy)
                        if listening and segment >= 40 and replies < 2:
                            listening = False
                            replies += 1
                            cancelled.clear()
                            worker = threading.Thread(target=reply, args=(replies,))
                            worker.start()
                        continue
                    if opcode != 1:
                        continue
                    value = json.loads(data)
                    kind = value.get('type')
                    if kind == 'hello':
                        declared_frame_ms = value['audio_params']['frame_duration']
                        assert declared_frame_ms in (20, 40, 60)
                        message({'type': 'hello', 'transport': 'websocket', 'session_id': 'device-fixture', 'audio_params': {'format': 'opus', 'sample_rate': 16000, 'channels': 1, 'frame_duration': 20}})
                        event('hello')
                    elif kind == 'listen':
                        listening = value.get('state') == 'start'
                        segment = 0
                        event('listen', state=value.get('state'))
                    elif kind == 'abort':
                        during_tts = tts_active.is_set()
                        cancelled.set()
                        if worker:
                            worker.join(timeout=2)
                        event('abort', during_tts=during_tts, number=replies)
            except (EOFError, OSError):
                pass
            except Exception as exc:
                event('error', detail=str(exc))
            finally:
                cancelled.set()
                if worker:
                    worker.join(timeout=2)
                lib.opus_decoder_destroy(decoder)
                event('capture_summary', packets=packets, samples=samples, nonzero_packets=nonzero, rms=math.sqrt(squares / samples) if samples else 0)

    class Server(socketserver.ThreadingTCPServer):
        allow_reuse_address = True
        daemon_threads = True

    with Server((args.bind, args.port), Handler) as server:
        event('ready', port=server.server_address[1])
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass


if __name__ == '__main__':
    main()
