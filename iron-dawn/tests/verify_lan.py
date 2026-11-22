#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Run a host and a guest in two official PXA product simulators."""
import argparse
from contextlib import ExitStack
import math
import os
from pathlib import Path
import re
import socket
import subprocess
import tempfile
import time

from runtime_geometry import column_center, compact_layout


class Player:
    def __init__(self, args, directory, control, width, height):
        self.log = directory / "run.log"
        self.control = control
        self.width, self.height = width, height
        directory.mkdir()
        (directory / "state").mkdir()
        with self.log.open("w") as stream:
            self.process = subprocess.Popen([
                args.simulator, "--package", args.package,
                "--publisher-key", args.publisher_key, "--state-root", directory / "state",
                "--width", str(width), "--height", str(height), "--density-dpi", "160",
                "--safe-insets", "0,0,0,0", "--corner-radius", "0",
                "--pxadb-control-socket", control,
            ], env=dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy",
                        PXA_SIM_NAV_MODE="gesture-no-bar"), stdout=stream, stderr=subprocess.STDOUT)
        self.w = self.h = 0

    def close(self):
        if self.process.poll() is None:
            self.process.terminate()
        try:
            self.process.wait(timeout=8)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait()

    def wait(self, pattern, *, start=0, timeout=15):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            content = self.log.read_text()
            match = re.search(pattern, content[start:])
            if match:
                return match
            assert self.process.poll() is None, content[-2500:]
            time.sleep(.1)
        raise AssertionError((pattern, self.log.read_text()[-3000:]))

    def command(self, value):
        with socket.socket(socket.AF_UNIX) as client:
            client.settimeout(15)
            client.connect(os.fspath(self.control))
            client.sendall((value + "\n").encode())
            with client.makefile("rb") as stream:
                header = stream.readline()
                if header.startswith(b"PNG "):
                    body = stream.read(int(header.split()[1]))
                    assert len(body) == int(header.split()[1])
                    return body
                assert header == b"OK\n", (value, header)

    def tap(self, x, y):
        self.command(f"TAP {round(x*self.width/self.w)} {round(y*self.height/self.h)}")
        time.sleep(.18)

    def shot(self, path):
        path.write_bytes(self.command("SCREENSHOT"))

    def ready(self):
        self.wait("Iron Dawn ready", timeout=30)
        display = self.wait(r"Iron Dawn display \d+x\d+ canvas=(\d+)x(\d+).*world=(\d+) top=(\d+) bottom=(\d+)")
        self.w, self.h, self.world, self.top, self.bottom = map(int, display.groups())
        self.row = min(math.ceil(32*self.h/self.height), self.h//6) if self.width < 400 else 32
        self.column = min(760, self.w-48)
        self.left = (self.w-self.column)//2
        self.footer = self.h-self.row-12
        time.sleep(.3)

    def open_lan(self):
        menu_width = min(720, max(480, self.w*5//8))
        left = (self.w-menu_width)//2+24
        self.tap(left+(menu_width-40)*3/8, max(214, self.h-128)+65)
        self.tap(self.left+3*self.column/4, self.footer+self.row/2)

    def enter_address(self, address):
        top = 70
        begin = top+self.row+4
        step = (self.footer-begin-6)//3
        for digit in address:
            i = "123456789.0:".index(digit)
            self.tap(self.left+self.column*((i%4)+.5)/4, begin+step*(i//4+.5))
        self.tap(self.left+5*self.column/6, self.footer+self.row/2)

    def saves(self):
        return {p.name: p.read_bytes() for p in self.log.parent.glob("state/private-files/**/*.save")}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for arg in ("simulator", "package", "publisher-key", "output"):
        parser.add_argument("--"+arg, type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True)
    with ExitStack() as stack:
        control = Path(stack.enter_context(tempfile.TemporaryDirectory(prefix="iron-lan-")))
        host = Player(args, args.output/"host", control/"host.sock", 800, 480)
        stack.callback(host.close)
        guest = Player(args, args.output/"guest", control/"guest.sock", 320, 240)
        stack.callback(guest.close)
        host.ready()
        guest.ready()
        # Make a solo checkpoint on both clients before testing room activity.
        for player in (host, guest):
            player.tap(player.w/2-48, max(214, player.h-128)+18)
            player.wait("Iron Dawn saved")
            player.command("KEY BACK")
            time.sleep(.25)
        host_save, guest_save = host.saves(), guest.saves()
        assert host_save and guest_save

        host.open_lan()
        host.shot(args.output/"host-menu.png")
        host.tap(host.left+host.column/4, 134)
        step = min(122, (host.footer-74-12)//3)
        host.tap(host.w/2, 74+step*1.5)  # 礁湾海防
        host.tap(host.left+3*host.column/4, 170)  # 灰塔守备军
        address = host.wait(r"Iron Dawn LAN room ([\d.:]+)").group(1)
        host.shot(args.output/"room.png")

        guest.open_lan()
        guest.tap(guest.left+3*guest.column/4, 160)
        guest.shot(args.output/"join.png")
        guest.enter_address(address)
        host.wait("Iron Dawn LAN teammate ready")
        host.tap(host.left+3*host.column/4, host.footer+host.row/2)
        host.wait("Iron Dawn LAN battle host")
        guest.wait("Iron Dawn LAN battle guest")
        host.shot(args.output/"host-battle.png")
        guest.shot(args.output/"guest-battle.png")

        # Initial selection includes the friendly army. Click open ground.
        guest.tap(guest.world*.55, guest.top+(guest.bottom-guest.top)*.62)
        host.wait("Iron Dawn LAN teammate order")
        layout = compact_layout(guest.w, guest.h, guest.height)
        commands_y = layout.top+5+layout.row/2
        guest.tap(column_center(guest.w, 4, 5), commands_y)
        guest.shot(args.output/"guest-tactics.png")
        start = host.log.stat().st_size
        guest.tap(column_center(guest.w, 3, 5), commands_y)
        host.wait("Iron Dawn LAN teammate order", start=start)

        # Leave the host menu open while the wave starts, and verify that
        # the guest sees enemies from the ongoing host simulation.
        host.command("KEY BACK")
        time.sleep(.25)
        host.shot(args.output/"host-in-game-menu.png")
        deadline = time.monotonic()+70
        while time.monotonic() < deadline:
            start = guest.log.stat().st_size
            guest.tap(column_center(guest.w, 3, 5), commands_y)
            state = guest.wait(r"screen=3 time=(\d+).*enemy=(\d+)", start=start)
            if int(state.group(2)) > 0:
                assert int(state.group(1)) >= 54
                break
            time.sleep(2)
        else:
            raise AssertionError("The guest never received the first enemy wave")
        guest.shot(args.output/"guest-first-wave.png")
        row, line = 32, 18
        leave = (host.h-(3*row+2*line+62))//2+2*line+44+2*row+row/2
        host.tap(host.w/2, leave)
        guest.wait("Iron Dawn LAN ended")
        guest.shot(args.output/"guest-disconnected.png")
        assert host.saves() == host_save and guest.saves() == guest_save
        print("PASS: direct two-player room, shared battle, remote commands, first wave, disconnect, solo save isolation")


if __name__ == "__main__":
    main()
