"""Benchmark the 296x240 pai-touch system-input fixture.

Install pxa-pinyin-ime and pxa-cpp-system-input, and select pxa-pinyin-ime first.
Reboot before invoking for a cold font-cache run. Screenshots are outside the
local heap-minimum windows. Pointer ACKs are not touch-to-photon measurements.
"""

import argparse
import json
import pathlib
import sys
import time


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    parser.add_argument("--pxadb-dir", type=pathlib.Path)
    parser.add_argument("--two-row", action="store_true", help="Use the redesigned spelling/word rails; the typed key sequence is identical")
    parser.add_argument("--phone-layout", action="store_true", help="Mobile four-column nine-key and fixed footer (296x240 only)")
    args = parser.parse_args()
    if args.pxadb_dir is None:
        for ancestor in pathlib.Path(__file__).resolve().parents:
            candidate = ancestor / "tools/pxadb"
            if (candidate / "pxadb.py").is_file():
                args.pxadb_dir = candidate
                break
    if args.pxadb_dir is None:
        parser.error("Specify --pxadb-dir pointing to pxa-projects/tools/pxadb")
    return args


class Benchmark:
    def __init__(self, client, output, pxadb, two_row=False, phone_layout=False):
        self.two_row = two_row
        self.phone_layout = phone_layout
        self.client = client
        self.output = output
        self.pxadb = pxadb
        self.started = time.monotonic()
        self.rows = []
        self.local_memory = False

    def request(self, command):
        begin = time.monotonic()
        frames = self.client.request(command, timeout=50)
        self.rows.append({
            "command": command,
            "start_ms": round((begin - self.started) * 1000, 3),
            "end_ms": round((time.monotonic() - self.started) * 1000, 3),
            "frames": [{"kind": f.kind, "payload": f.payload} for f in frames],
        })
        if command == "MEMORY START":
            self.local_memory = True
        elif command == "MEMORY STOP":
            self.local_memory = False
        return frames

    def tap(self, x, y):
        # The driver ACK precedes LVGL sampling. Keep DOWN/UP far enough apart.
        self.request(f"INPUT POINTER DOWN {x} {y} 0")
        time.sleep(0.15)
        self.request(f"INPUT POINTER UP {x} {y} 0")
        self.request("INPUT SYNC")
        time.sleep(0.4)

    def timing(self):
        payload = next(f.payload for f in self.request("IME") if f.kind == "TIMING")
        return dict(field.split("=", 1) for field in payload.split(";"))

    def check_ready(self):
        timing = self.timing()
        assert timing["keyboard_drawn"] == "1", "Keyboard did not draw"
        assert timing["candidates_ready"] == "1", "Provider did not acknowledge RESET"

    def screenshot(self, name):
        assert not self.local_memory, "Screenshot would contaminate the peak window"
        for attempt in range(3):
            try:
                frames = self.client.request("SCREENSHOT JPEG", timeout=30)
                self.pxadb.write_screenshot(
                    self.pxadb.screenshot_capture(frames), self.output / f"{name}.jpg"
                )
                return
            except self.pxadb.PxaDbError:
                if attempt == 2:
                    raise
                time.sleep(0.1)

    def run(self):
        for attempt in range(60):
            try:
                capabilities = self.request("INPUT CAPABILITIES")
                break
            except self.pxadb.PxaDbRejectedError:
                time.sleep(1)
        else:
            raise RuntimeError("Input adapter did not become ready")
        assert any("width=296;height=240;" in f.payload for f in capabilities)
        assert any(f.kind == "SELECTED" and f.payload.endswith(":pxa-pinyin-ime")
                   for f in self.request("IME")), "Select pxa-pinyin-ime first"
        self.request("INPUT KEY HOME")
        time.sleep(0.2)
        self.request("INPUT POINTER DOWN 148 216 0")
        time.sleep(0.12)
        self.request("INPUT POINTER MOVE 148 70 0")
        time.sleep(0.12)
        self.request("INPUT POINTER UP 148 70 0")
        self.request("INPUT SYNC")
        self.request("PACKAGE run pxa-cpp-system-input")
        for attempt in range(30):
            if any(f.kind == "PKG" and ":pxa-cpp-system-input\t" in f.payload
                   and "active=1" in f.payload for f in self.request("PACKAGES")):
                break
            time.sleep(0.2)
        else:
            raise RuntimeError("Foreground fixture did not start")
        time.sleep(1)
        self.request("MEMORY")
        self.request("MEMORY START")
        self.tap(35, 55)
        time.sleep(4)
        self.check_ready()
        self.request("MEMORY STOP")
        self.screenshot("cold-ready")
        # K9 is ambiguous: commit the first result of 64426, whatever its words.
        middle=163 if self.two_row else 153
        upper=142 if self.two_row else 128
        digits=[(179,163),(50,163),(50,163),(115,142),(179,163)] if self.phone_layout else [(218,middle),(69,middle),(69,middle),(148,upper),(218,middle)]
        for x, y in digits:
            self.tap(x, y)
        self.timing()
        self.screenshot("chinese-candidates")
        self.tap(43,119) if self.phone_layout else self.tap(55,119) if self.two_row else self.tap(133,102)
        self.screenshot("chinese-committed")
        if self.phone_layout:
            self.tap(109,209)
            for x,y in [(32,163),(173,184),(116,184)]: self.tap(x,y)
        else:
            self.tap(54,209); self.tap(71,142)
            for x in [55,101,147]: self.tap(x,107)
        self.screenshot("english")
        delete_x,delete_y=(264,163) if self.phone_layout else (147,209)
        self.request(f"INPUT POINTER DOWN {delete_x} {delete_y} 0")
        time.sleep(1.1)
        self.request(f"INPUT POINTER UP {delete_x} {delete_y} 0")
        self.request("INPUT SYNC")
        time.sleep(0.5)
        self.screenshot("held-delete")
        hide_x=258 if self.phone_layout else 240
        self.tap(hide_x,209)
        time.sleep(0.4)
        self.request("MEMORY")
        self.request("MEMORY START")
        for cycle in range(3):
            self.tap(35, 55)
            time.sleep(4)
            self.check_ready()
            self.request("MEMORY")
            self.tap(hide_x,209)
            time.sleep(0.5)
            self.request("MEMORY")
        self.request("MEMORY STOP")


def main():
    args = arguments()
    sys.path.insert(0, str(args.pxadb_dir))
    import pxadb

    args.output.mkdir(parents=True, exist_ok=True)
    with pxadb.PxaDbClient(args.port, timeout=15) as client:
        client.device_info = client.hello()
        bench = Benchmark(client,args.output,pxadb,args.two_row,args.phone_layout)
        passed = False
        try:
            bench.run()
            passed = True
        finally:
            try:
                if bench.local_memory:
                    bench.request("MEMORY STOP")
                packages = bench.request("PACKAGES")
                if any(f.kind == "PKG" and ":pxa-cpp-system-input\t" in f.payload
                       and "active=1" in f.payload for f in packages):
                    bench.request("PACKAGE stop pxa-cpp-system-input")
                    time.sleep(0.5)
                bench.request("MEMORY")
            except Exception:
                passed = False
                raise
            finally:
                (args.output / "measurement.json").write_text(
                    json.dumps({"passed": passed, "rows": bench.rows},
                               ensure_ascii=False, indent=2), encoding="utf-8"
                )
    print(f"Benchmark passed; measurements and screenshots: {args.output}")


if __name__ == "__main__":
    main()
