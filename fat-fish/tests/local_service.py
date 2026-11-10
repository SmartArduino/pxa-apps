#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Exercise the Responses adapter against bounded HTTP fixtures."""
import importlib.util
import json
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import threading
import unittest
from unittest.mock import patch
from types import SimpleNamespace
from collections import OrderedDict
from concurrent.futures import ThreadPoolExecutor

spec = importlib.util.spec_from_file_location("dsh_local", Path(__file__).parents[1] / "tools/dsh_local.py")
local = importlib.util.module_from_spec(spec)
spec.loader.exec_module(local)
bridge_spec = importlib.util.spec_from_file_location("dsh_bridge", Path(__file__).parents[1] / "tools/dsh_bridge.py")
bridge = importlib.util.module_from_spec(bridge_spec)
bridge_spec.loader.exec_module(bridge)


class Gateway(BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def do_POST(self):
        self.server.request = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
        self.send_response(self.server.status)
        self.send_header("Content-Type", self.server.kind)
        self.send_header("Content-Length", str(len(self.server.body)))
        self.end_headers()
        self.wfile.write(self.server.body)

    def do_GET(self):
        self.server.path = self.path
        self.server.headers = self.headers
        self.send_response(self.server.status)
        self.send_header("Content-Length", str(len(self.server.body)))
        self.end_headers()
        self.wfile.write(self.server.body)


class ResponsesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = ThreadingHTTPServer(("127.0.0.1", 0), Gateway)
        cls.thread = threading.Thread(target=cls.server.serve_forever)
        cls.thread.start()
        cls.config = {"base_url": "http://127.0.0.1:" + str(cls.server.server_port),
                      "api_key": "fixture-key", "model": "gpt-6-luna"}

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.thread.join()
        cls.server.server_close()

    def setUp(self):
        self.server.status = 200
        self.server.kind = "text/event-stream"

    def stream(self, events):
        self.server.body = b"".join(("data: " + json.dumps(event) + "\n\n").encode() for event in events)
        return local.generate(self.config, [{"role": "user", "content": "你好"}])

    def test_completed_stream(self):
        text = self.stream([{"type": "response.output_text.delta", "delta": "你好"},
                            {"type": "response.output_text.delta", "delta": "！"},
                            {"type": "response.completed", "response": {"status": "completed"}}])
        self.assertEqual(text, "你好！")
        self.assertEqual(self.server.request["model"], "gpt-6-luna")
        self.assertFalse(self.server.request["store"])
        self.assertTrue(self.server.request["stream"])

    def test_partial_reply_is_rejected(self):
        for ending in ([], [{"type": "response.failed"}], [{"type": "response.incomplete"}]):
            with self.subTest(ending=ending), self.assertRaises(RuntimeError):
                self.stream([{"type": "response.output_text.delta", "delta": "部分文字"}] + ending)

    def test_json_response(self):
        self.server.kind = "application/json"
        self.server.body = json.dumps({"status": "completed", "output": [
            {"type": "reasoning", "summary": []},
            {"type": "message", "content": [{"type": "output_text", "text": "你好"}]}]}).encode()
        self.assertEqual(local.generate(self.config, [{"role": "user", "content": "你好"}]), "你好")

    def test_http_error(self):
        self.server.status = 503
        self.server.body = b"unavailable"
        with self.assertRaises(RuntimeError):
            local.generate(self.config, [{"role": "user", "content": "你好"}])

    def test_speech_chunks(self):
        text = "你好，主人！" + "鱼" * 40 + "。"
        chunks = list(local.speech_chunks(text))
        self.assertEqual("".join(chunks), text)
        self.assertTrue(all(0 < len(chunk) <= 18 for chunk in chunks))
        self.assertEqual(list(local.speech_chunks("鱼" * 18 + "。")), ["鱼" * 18])
        self.assertEqual(list(local.speech_chunks("……！")), [])


class SpeechSessionTest(unittest.TestCase):
    def setUp(self):
        self.accept = False
        owner = self

        class Recognizer:
            def __init__(self, *args):
                pass

            def AcceptWaveform(self, data):
                accepted = owner.accept
                owner.accept = False
                return accepted

            def Result(self):
                return '{"text":"今天 天气 怎么样"}'

            def FinalResult(self):
                return '{"text":""}'

        modules = patch.dict("sys.modules", {"vosk": SimpleNamespace(KaldiRecognizer=Recognizer)})
        modules.start()
        self.addCleanup(modules.stop)
        self.speech = local.Speech.__new__(local.Speech)
        self.speech.asr = None
        self.speech.asr_lock = threading.Lock()
        self.speech.sessions = {}
        self.speech.decode_audio = lambda document: bytes(32000)

    def test_completed_utterance_releases_session(self):
        self.accept = True
        result = self.speech.listen({"session": "turn-1"})
        self.assertEqual(result, {"text": "今天天气怎么样", "finished": True})
        self.assertFalse(self.speech.sessions)

    def test_silence_resets_after_twenty_seconds(self):
        for second in range(20):
            result = self.speech.listen({"session": "quiet"})
            self.assertEqual(result["finished"], second == 19)
        self.assertFalse(self.speech.sessions)

    def test_capacity_and_expiry(self):
        for index in range(4):
            self.speech.listen({"session": str(index)})
        with self.assertRaises(RuntimeError):
            self.speech.listen({"session": "extra"})
        for session in self.speech.sessions.values():
            session["at"] -= 46
        self.speech.listen({"session": "extra"})
        self.assertEqual(list(self.speech.sessions), ["extra"])


class SpeechCacheTest(unittest.TestCase):
    def setUp(self):
        self.speech = local.Speech.__new__(local.Speech)
        self.speech.speech_cache = OrderedDict()
        self.speech.cache_lock = threading.Lock()

    def test_prefetch_and_playback_share_synthesis(self):
        started, release = threading.Event(), threading.Event()
        calls = []
        def synthesize(text):
            calls.append(text)
            started.set()
            release.wait(2)
            return b"audio"
        self.speech.synthesize = synthesize
        with ThreadPoolExecutor(max_workers=2) as workers:
            first = workers.submit(self.speech.speak, {"text": "你好"})
            self.assertTrue(started.wait(2))
            second = workers.submit(self.speech.speak, {"text": "你好"})
            release.set()
            self.assertEqual(first.result(), b"audio")
            self.assertEqual(second.result(), b"audio")
        self.assertEqual(calls, ["你好"])

    def test_failed_synthesis_can_retry(self):
        def fail(text):
            raise RuntimeError("fixture")
        self.speech.synthesize = fail
        with self.assertRaises(RuntimeError):
            self.speech.speak({"text": "你好"})
        self.assertFalse(self.speech.speech_cache)
        self.speech.synthesize = lambda text: b"retry"
        self.assertEqual(self.speech.speak({"text": "你好"}), b"retry")

    def test_cache_is_bounded(self):
        self.speech.synthesize = lambda text: text.encode()
        for index in range(12):
            self.speech.speak({"text": str(index)})
        self.assertEqual(list(self.speech.speech_cache), [str(i) for i in range(4, 12)])


class SpeechPrefetchTest(unittest.TestCase):
    def setUp(self):
        self.handler = bridge.Handler.__new__(bridge.Handler)
        self.slots = threading.BoundedSemaphore(2)
        self.handler.server = SimpleNamespace(voice_port=7344, voice_token="fixture", speech_slots=self.slots)

    def assert_slots_available(self):
        self.assertTrue(self.slots.acquire(blocking=False))
        self.assertTrue(self.slots.acquire(blocking=False))
        self.assertFalse(self.slots.acquire(blocking=False))

    def test_invalid_unicode_does_not_consume_prefetch_slot(self):
        self.handler.prepare_speech(b'{"ok":true,"text":"\\ud800"}')
        self.assert_slots_available()

    def test_thread_failure_preserves_chat_and_releases_slot(self):
        with patch.object(bridge.threading, "Thread") as worker:
            worker.return_value.start.side_effect = RuntimeError("fixture")
            self.handler.prepare_speech(b'{"ok":true,"text":"hello"}')
        self.assert_slots_available()


class BridgeStateTest(unittest.TestCase):
    def setUp(self):
        self.server = ThreadingHTTPServer(("127.0.0.1", 0), Gateway)
        self.worker = threading.Thread(target=self.server.serve_forever)
        self.worker.start()
        self.server.status = 200
        self.handler = bridge.Handler.__new__(bridge.Handler)
        self.reply = b'{"ok":true,"text":"hello"}'

    def tearDown(self):
        self.server.shutdown()
        self.worker.join()
        self.server.server_close()

    def bundle(self):
        return self.handler.attach_state(self.reply, self.server.server_port,
                                         {"x-dsh-pet-token": "fixture"}, "pet=main.pet&surface=any")

    def test_snapshot_keeps_only_selected_pet_animation(self):
        animation = {"counter": 42, "data": {"name": "轻快摇摆舞"}}
        self.server.body = json.dumps({"pets": {"main.pet": {"anim": animation, "private": "unused"},
                                                 "other": {"private": "unused"}}}).encode()
        document = json.loads(self.bundle())
        self.assertEqual(document, {"ok": True, "text": "hello", "pxa_state": {
            "pets": {"main.pet": {"anim": animation}}}})
        self.assertEqual(self.server.path, "/dsh-pet-7340/state?pet=main.pet&surface=any")
        self.assertEqual(self.server.headers["x-dsh-pet-token"], "fixture")

    def test_state_failure_preserves_original_reply(self):
        for body in (b'{', b'null', b'{"pets":{}}', b'x' * (bridge.LIMIT + 1)):
            with self.subTest(body=body[:40]):
                self.server.body = body
                self.assertEqual(self.bundle(), self.reply)
        self.server.status = 503
        self.server.body = b'{}'
        self.assertEqual(self.bundle(), self.reply)

    def test_timeout_preserves_original_reply(self):
        with patch.object(bridge.http.client, "HTTPConnection") as connection:
            connection.return_value.request.side_effect = TimeoutError("fixture")
            self.assertEqual(self.bundle(), self.reply)
            connection.return_value.close.assert_called_once()


if __name__ == "__main__":
    unittest.main()
