#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Local speech and Responses adapter for the dsh-pet companion service."""
import argparse
import base64
import binascii
from collections import OrderedDict
from concurrent.futures import Future
import hmac
import http.client
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import re
import subprocess
import threading
import time
from urllib.parse import urlsplit


def response_text(response):
    return "".join(part.get("text", "")
                   for item in response.get("output", []) if item.get("type") == "message"
                   for part in item.get("content", []) if part.get("type") == "output_text")


def generate(config, messages):
    endpoint = urlsplit(config["base_url"])
    connection_type = http.client.HTTPSConnection if endpoint.scheme == "https" else http.client.HTTPConnection
    connection = connection_type(endpoint.hostname, endpoint.port, timeout=55)
    prefix = endpoint.path.rstrip("/")
    if not prefix.endswith("/v1"):
        prefix += "/v1"
    request = {"model": config["model"], "input": messages, "store": False,
               "stream": True, "max_output_tokens": 256,
               "reasoning": {"effort": "minimal"}}
    try:
        connection.request("POST", prefix + "/responses", json.dumps(request, ensure_ascii=False).encode(),
                           headers={"Content-Type": "application/json", "Authorization": "Bearer " + config["api_key"]})
        response = connection.getresponse()
        if response.status != 200:
            raise RuntimeError(f"Responses request failed (HTTP {response.status})")
        if "application/json" in response.getheader("Content-Type", ""):
            document = json.loads(response.read(262145))
            if document.get("status") != "completed":
                raise RuntimeError("Responses did not complete")
            text = response_text(document)
        else:
            parts = []
            complete = False
            received = 0
            for line in response:
                received += len(line)
                if received > 262144:
                    raise RuntimeError("Responses stream exceeded its limit")
                if not line.startswith(b"data: ") or line.strip() == b"data: [DONE]":
                    continue
                event = json.loads(line[6:])
                kind = event.get("type")
                if kind == "response.output_text.delta":
                    parts.append(event.get("delta", ""))
                if kind in ("error", "response.failed", "response.incomplete"):
                    raise RuntimeError("Responses generation failed")
                if kind == "response.completed":
                    complete = event.get("response", {}).get("status") == "completed"
                    if not parts:
                        parts.append(response_text(event.get("response", {})))
                    break
            if not complete:
                raise RuntimeError("Responses stream ended before completion")
            text = "".join(parts)
        if not text.strip():
            raise RuntimeError("Responses returned an empty reply")
        return text
    finally:
        connection.close()


def convert_audio(data, arguments):
    result = subprocess.run(["ffmpeg", "-nostdin", "-hide_banner", "-loglevel", "error"] + arguments,
                            input=data, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=20)
    if result.returncode:
        raise ValueError("Audio conversion failed")
    return result.stdout


def speech_chunks(text, limit=18):
    for sentence in re.findall(r"[^。！？!?\n]+[。！？!?\n]*|[。！？!?\n]+", text):
        for offset in range(0, len(sentence), limit):
            chunk = sentence[offset:offset + limit].strip()
            if any(character.isalnum() for character in chunk):
                yield chunk


class Speech:
    def __init__(self, config):
        import sherpa_onnx
        import vosk
        vosk.SetLogLevel(-1)
        root = Path(config["models_dir"]).expanduser()
        self.asr = vosk.Model(str(root / "vosk-model-small-cn-0.22"))
        model = root / "melo-zh"
        model_file = config.get("tts", {}).get("model", "model.int8.onnx")
        self.tts_phrase_limit = 32 if model_file == "model.onnx" else 18
        vits = sherpa_onnx.OfflineTtsVitsModelConfig(
            model=str(model / model_file), tokens=str(model / "tokens.txt"),
            lexicon=str(model / "lexicon.txt"), dict_dir=str(model / "dict"))
        settings = sherpa_onnx.OfflineTtsConfig(
            model=sherpa_onnx.OfflineTtsModelConfig(vits=vits, num_threads=2, provider="cpu"),
            rule_fsts=",".join(str(model / name) for name in ("date.fst", "number.fst", "phone.fst")))
        self.tts = sherpa_onnx.OfflineTts(settings)
        self.asr_lock = threading.Lock()
        self.tts_lock = threading.Lock()
        self.sessions = {}
        self.speech_cache = OrderedDict()
        self.cache_lock = threading.Lock()

    @staticmethod
    def decode_audio(document):
        encoded = document.get("audio")
        if not isinstance(encoded, str):
            raise ValueError("audio is required")
        try:
            audio = base64.b64decode(encoded, validate=True)
        except (ValueError, binascii.Error):
            raise ValueError("audio must be base64") from None
        if not 0 < len(audio) <= 768 * 1024 or audio[:4] not in (b"RIFF", b"OggS"):
            raise ValueError("Expected WAV or Ogg Opus audio")
        pcm = convert_audio(audio, ["-i", "pipe:0", "-t", "20", "-ar", "16000", "-ac", "1", "-f", "s16le", "pipe:1"])
        if len(pcm) > 640000:
            raise ValueError("Recording exceeds 20 seconds")
        return pcm

    def hear(self, document):
        import vosk
        pcm = self.decode_audio(document)
        with self.asr_lock:
            recognizer = vosk.KaldiRecognizer(self.asr, 16000)
            parts = []
            for offset in range(0, len(pcm), 8000):
                if recognizer.AcceptWaveform(pcm[offset:offset + 8000]):
                    parts.append(json.loads(recognizer.Result()).get("text", ""))
            parts.append(json.loads(recognizer.FinalResult()).get("text", ""))
        text = " ".join(part for part in parts if part)
        return re.sub(r"(?<=[\u3400-\u9fff])\s+(?=[\u3400-\u9fff])", "", text)

    def listen(self, document):
        import vosk
        session_id = document.get("session")
        if not isinstance(session_id, str) or not re.fullmatch(r"[A-Za-z0-9_-]{1,64}", session_id):
            raise ValueError("Invalid speech session")
        pcm = self.decode_audio(document)
        now = time.monotonic()
        with self.asr_lock:
            for key, value in list(self.sessions.items()):
                if now - value["at"] > 45:
                    del self.sessions[key]
            if session_id not in self.sessions:
                if len(self.sessions) >= 4:
                    raise RuntimeError("Too many active speech sessions")
                self.sessions[session_id] = {"recognizer": vosk.KaldiRecognizer(self.asr, 16000),
                                             "at": now, "samples": 0}
            session = self.sessions[session_id]
            recognizer = session["recognizer"]
            session["at"] = now
            session["samples"] += len(pcm) // 2
            results = []
            for offset in range(0, len(pcm), 8000):
                if recognizer.AcceptWaveform(pcm[offset:offset + 8000]):
                    results.append(json.loads(recognizer.Result()).get("text", ""))
            text = " ".join(part for part in results if part)
            finished = bool(text) or session["samples"] >= 320000
            if finished:
                text += " " + json.loads(recognizer.FinalResult()).get("text", "")
                del self.sessions[session_id]
            text = re.sub(r"(?<=[\u3400-\u9fff])\s+(?=[\u3400-\u9fff])", "", text.strip())
            return {"text": text, "finished": finished}

    def speak(self, document):
        text = document.get("text", "")
        if not isinstance(text, str) or not text.strip() or len(text) > 500:
            raise ValueError("text must contain 1..500 characters")
        text = text.strip()
        with self.cache_lock:
            future = self.speech_cache.get(text)
            owner = future is None
            if owner:
                if len(self.speech_cache) >= 8:
                    completed = next((key for key, value in self.speech_cache.items() if value.done()), None)
                    if completed is None:
                        raise RuntimeError("Speech synthesis is busy")
                    del self.speech_cache[completed]
                future = Future()
                self.speech_cache[text] = future
            self.speech_cache.move_to_end(text)
        if owner:
            try:
                future.set_result(self.synthesize(text))
            except Exception as error:
                future.set_exception(error)
                with self.cache_lock:
                    if self.speech_cache.get(text) is future:
                        del self.speech_cache[text]
                raise
        return future.result(timeout=55)

    def synthesize(self, text):
        import numpy as np
        # Bound each CPU inference so long utterances have predictable memory use.
        blocks = []
        samples = 0
        with self.tts_lock:
            for chunk in speech_chunks(text.strip(), self.tts_phrase_limit):
                audio = self.tts.generate(chunk, sid=0, speed=1.05)
                blocks.append((np.clip(audio.samples, -1, 1) * 32767).astype("<i2").tobytes())
                samples += len(audio.samples)
                if samples >= audio.sample_rate * 30:
                    break
        if not samples:
            raise ValueError("No pronounceable text")
        return convert_audio(b"".join(blocks), ["-f", "s16le", "-ar", str(audio.sample_rate), "-ac", "1", "-i", "pipe:0",
                                   "-t", "30", "-ar", "22050", "-ac", "1", "-codec:a", "libmp3lame",
                                   "-b:a", "32k", "-f", "mp3", "pipe:1"])


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, format, *args):
        pass

    def send(self, status, data, kind="application/json; charset=utf-8"):
        self.send_response(status)
        self.send_header("Content-Type", kind)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Connection", "close")
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        try:
            self.wfile.write(data)
        except (BrokenPipeError, ConnectionResetError):
            pass
        self.close_connection = True

    def json(self, status, document):
        self.send(status, json.dumps(document, ensure_ascii=False).encode())

    def authorized(self):
        supplied = self.headers.get("Authorization", "").encode()
        return hmac.compare_digest(supplied, ("Bearer " + self.server.config["local_token"]).encode())

    def do_GET(self):
        if self.path == "/health":
            self.json(200, {"ok": True})
        elif not self.authorized():
            self.json(401, {"ok": False, "reason": "unauthorized"})
        elif self.path.split("?")[0].endswith("/voice/voices"):
            self.json(200, {"ok": True, "voices": [{"id": "local-melo-zh", "name": "本地中文", "locale": "zh-CN"}]})
        else:
            self.json(404, {"ok": False, "reason": "not-found"})

    def do_POST(self):
        self.connection.settimeout(15)
        if not self.authorized():
            self.json(401, {"ok": False, "reason": "unauthorized"})
            return
        path = self.path.split("?")[0]
        started = time.monotonic()
        try:
            size = int(self.headers.get("Content-Length", "0"))
            if not 0 < size <= 1024 * 1024 or self.headers.get("Transfer-Encoding"):
                raise ValueError("Invalid request body size")
            data = self.rfile.read(size)
            if len(data) != size:
                raise ValueError("Incomplete request body")
            document = json.loads(data)
            if not isinstance(document, dict):
                raise ValueError("Expected a JSON object")
            if path == "/v1/chat/completions":
                if document.get("model") != self.server.config["llm"]["model"]:
                    raise ValueError("Model is not configured")
                messages = document.get("messages")
                if not isinstance(messages, list) or not messages:
                    raise ValueError("messages is required")
                text = generate(self.server.config["llm"], messages)
                chunk = {"id": "dsh-local-response", "object": "chat.completion.chunk", "created": int(time.time()),
                         "model": self.server.config["llm"]["model"],
                         "choices": [{"index": 0, "delta": {"content": text}, "finish_reason": None}]}
                if document.get("stream"):
                    wire = ("data: " + json.dumps(chunk, ensure_ascii=False) + "\n\ndata: [DONE]\n\n").encode()
                    self.send(200, wire, "text/event-stream")
                else:
                    chunk["object"] = "chat.completion"
                    chunk["choices"] = [{"index": 0, "message": {"role": "assistant", "content": text}, "finish_reason": "stop"}]
                    self.json(200, chunk)
            elif path.endswith("/voice/hear"):
                result = self.server.speech.listen(document) if "session" in document else {"text": self.server.speech.hear(document)}
                self.json(200, {"ok": True, **result})
            elif path.endswith("/voice/speak"):
                self.send(200, self.server.speech.speak(document), "audio/mpeg")
            else:
                self.json(404, {"ok": False, "reason": "not-found"})
            print(f"{path} completed seconds={time.monotonic() - started:.3f}", flush=True)
        except (ValueError, KeyError, TypeError):
            self.json(400, {"ok": False, "reason": "bad-request", "message": "请求格式或音频无效"})
        except (OSError, RuntimeError, subprocess.SubprocessError, http.client.HTTPException) as error:
            print(f"{path} failed type={type(error).__name__}", flush=True)
            self.json(502, {"ok": False, "reason": "generate-error", "message": "本地语音或模型服务暂时不可用"})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path, default=Path.home() / ".config/pxa-fat-fish/voice.json")
    parser.add_argument("--port", type=int, default=7344)
    args = parser.parse_args()
    if args.config.stat().st_mode & 0o077:
        parser.error("The private configuration must have mode 0600")
    config = json.loads(args.config.read_text())
    speech = Speech(config)
    with ThreadingHTTPServer(("127.0.0.1", args.port), Handler) as server:
        server.daemon_threads = True
        server.config = config
        server.speech = speech
        print(f"Local speech and Responses adapter ready on 127.0.0.1:{args.port}", flush=True)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass


if __name__ == "__main__":
    main()
