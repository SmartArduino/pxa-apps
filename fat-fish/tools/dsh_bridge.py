#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Expose dsh-pet chat/state to a PXA device, with a separate LAN token.

The upstream standalone service stays on loopback. Its rotating token is read
from standalone-runtime.json for each request. Config and logs stay outside
the source tree. Only Python's standard library is needed.
"""
import argparse
import hmac
import http.client
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import re
import socket
import threading
from urllib.parse import parse_qs, urlsplit

LIMIT = 32768
VOICE_LIMIT = 1024 * 1024


class Bridge(ThreadingHTTPServer):
    daemon_threads = True
    block_on_close = False


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, format, *args):
        # Requests can contain user text or authentication data.
        pass

    def result(self, status, body, content_type="application/json; charset=utf-8"):
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("Connection", "close")
        self.end_headers()
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            pass
        self.close_connection = True

    def error(self, status, reason):
        self.result(status, json.dumps({"ok": False, "reason": reason}).encode())

    def prepare_speech(self, data):
        if not self.server.voice_port:
            return
        try:
            document = json.loads(data)
            text = document.get("text")
            if document.get("ok") is not True or not isinstance(text, str) or not text.strip():
                return
            text = text.encode("utf-8")[:240].decode("utf-8", errors="ignore")
        except (ValueError, AttributeError):
            return
        if not self.server.speech_slots.acquire(blocking=False):
            return
        def prepare():
            connection = http.client.HTTPConnection("127.0.0.1", self.server.voice_port, timeout=45)
            try:
                connection.request("POST", "/dsh-pet-7340/voice/speak", json.dumps({"text": text}).encode(),
                                   headers={"Content-Type": "application/json", "Authorization": "Bearer " + self.server.voice_token})
                response = connection.getresponse()
                response.read(131073)
            except (OSError, ValueError, http.client.HTTPException):
                pass
            finally:
                connection.close()
                self.server.speech_slots.release()
        try:
            threading.Thread(target=prepare, daemon=True).start()
        except RuntimeError:
            self.server.speech_slots.release()

    def attach_state(self, data, port, headers, query):
        """Keep the animation snapshot beside the reply to avoid a device round trip."""
        connection = http.client.HTTPConnection("127.0.0.1", port, timeout=.25)
        try:
            document = json.loads(data)
            if not isinstance(document, dict) or document.get("ok") is not True:
                return data
            pet = parse_qs(query, max_num_fields=16).get("pet", ["main"])[0]
            connection.request("GET", "/dsh-pet-7340/state?" + query, headers=headers)
            response = connection.getresponse()
            snapshot = response.read(LIMIT + 1)
            if response.status != 200 or len(snapshot) > LIMIT:
                return data
            selected = json.loads(snapshot).get("pets", {}).get(pet)
            if not isinstance(selected, dict):
                return data
            document["pxa_state"] = {"pets": {pet: {"anim": selected.get("anim")}}}
            bundled = json.dumps(document, ensure_ascii=False, separators=(",", ":")).encode()
            return bundled if len(bundled) <= LIMIT else data
        except (OSError, ValueError, AttributeError, http.client.HTTPException):
            return data
        finally:
            connection.close()

    def forward(self):
        self.connection.settimeout(10)
        supplied = self.headers.get("Authorization", "").encode("utf-8")
        if not hmac.compare_digest(supplied, ("Bearer " + self.server.token).encode("ascii")):
            self.error(401, "bridge-unauthorized")
            return
        path = urlsplit(self.path)
        if path.scheme or path.netloc or path.fragment or not re.fullmatch(r"/dsh-pet-7340/(chat|state|voice/(hear|speak|voices))", path.path):
            self.error(404, "bridge-route-unavailable")
            return
        read_only = path.path.endswith(("/state", "/voice/voices"))
        post_only = path.path.endswith(("/voice/hear", "/voice/speak"))
        if (self.command == "POST" and read_only) or (self.command == "GET" and post_only):
            self.error(405, "bridge-method-unavailable")
            return
        if self.headers.get("Transfer-Encoding"):
            self.error(400, "bridge-invalid-body")
            return
        try:
            length = int(self.headers.get("Content-Length", "0"))
            request_limit = VOICE_LIMIT if path.path.endswith("/voice/hear") else 8192
            if not 0 <= length <= request_limit:
                self.error(413, "bridge-body-too-large")
                return
            body = self.rfile.read(length)
            if len(body) != length:
                self.error(400, "bridge-incomplete-body")
                return
            voice = "/voice/" in path.path and self.server.voice_port
            runtime = json.loads(self.server.runtime.read_text())
            port = self.server.voice_port if voice else int(runtime["port"])
            headers = {"Content-Type": "application/json", "Accept": "application/json"}
            if voice:
                headers["Authorization"] = "Bearer " + self.server.voice_token
            else:
                headers["x-dsh-pet-token"] = runtime["token"]
            connection = http.client.HTTPConnection("127.0.0.1", port, timeout=65)
            try:
                connection.request(self.command, self.path, body=body, headers=headers)
                response = connection.getresponse()
                response_limit = VOICE_LIMIT if path.path.endswith("/voice/speak") else LIMIT
                data = response.read(response_limit + 1)
                if len(data) > response_limit:
                    self.error(502, "bridge-response-too-large")
                    return
                if path.path.endswith("/chat") and self.command == "POST" and response.status == 200:
                    self.prepare_speech(data)
                    data = self.attach_state(data, port, headers, path.query)
                self.result(response.status, data, response.getheader("Content-Type", "application/json"))
                print(f"{self.command} {path.path} -> {response.status} ({len(data)} bytes)", flush=True)
            finally:
                connection.close()
        except (OSError, ValueError, KeyError, http.client.HTTPException, socket.timeout):
            self.error(502, "bridge-upstream-unavailable")

    do_GET = forward
    do_POST = forward


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path, required=True,
                        help="Private JSON containing bind, port, token and runtime_file")
    args = parser.parse_args()
    config = json.loads(args.config.read_text())
    token = config["token"]
    if not isinstance(token, str) or not re.fullmatch(r"[A-Za-z0-9_-]{32,128}", token):
        parser.error("token must contain 32..128 base64url characters")
    with Bridge((config["bind"], int(config.get("port", 7341))), Handler) as server:
        server.token = token
        server.runtime = Path(config["runtime_file"]).expanduser().resolve()
        server.voice_port = int(config.get("voice_port", 0))
        server.voice_token = config.get("voice_token", "")
        server.speech_slots = threading.BoundedSemaphore(2)
        print(f"dsh-pet PXA bridge listening on {server.server_address[0]}:{server.server_address[1]}", flush=True)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass


if __name__ == "__main__":
    main()
