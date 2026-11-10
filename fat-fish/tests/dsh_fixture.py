#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Loopback-only model fixture for testing the real dsh-pet standalone server.

All replies are visibly marked as protocol tests. No external model is used.
Point a separate test DSH_HOME at http://127.0.0.1:7350/v1 with a dummy key.
"""
import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import time


class Handler(BaseHTTPRequestHandler):
    def log_message(self, format, *args):
        pass

    def do_POST(self):
        length = int(self.headers.get("Content-Length", "0"))
        if self.path != "/v1/chat/completions" or not 0 < length <= 131072:
            self.send_error(400)
            return
        request = json.loads(self.rfile.read(length))
        messages = request.get("messages", [])
        users = [m["content"] for m in messages if m.get("role") == "user"]
        prompt = users[-1] if users else ""
        with self.server.log.open("a") as log:
            log.write(json.dumps({"user_count": len(users), "prompt": prompt}, ensure_ascii=False) + "\n")
        if "等待" in prompt or "wait" in prompt.lower():
            time.sleep(3)
        text = f'【协议测试】收到啦，陪你聊聊今天。\n这是第 {len(users)} 轮对话。[动作:轻快摇摆舞]'
        if "长回复" in prompt:
            text = "【协议测试】" + "今天也要开心呀。" * 400
        event = {"id": "pxa-protocol-fixture", "object": "chat.completion.chunk", "created": 0,
                 "model": "protocol-fixture", "choices": [{"index": 0, "delta": {"content": text}, "finish_reason": None}]}
        body = ("data: " + json.dumps(event, ensure_ascii=False) + "\n\ndata: [DONE]\n\n").encode()
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            pass


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=7350)
    parser.add_argument("--log", type=Path, required=True)
    args = parser.parse_args()
    if "local/artifacts" not in str(args.log.resolve()):
        parser.error("write fixture logs under local/artifacts")
    args.log.parent.mkdir(parents=True, exist_ok=True)
    with ThreadingHTTPServer(("127.0.0.1", args.port), Handler) as server:
        server.log = args.log
        server.serve_forever()


if __name__ == "__main__":
    main()
