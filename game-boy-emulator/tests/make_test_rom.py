#!/usr/bin/env python3
"""Builds a tiny Game Boy cartridge used by the host and simulator tests.

The image boots into a loop that paints a striped background and rotates the
background palette every frame, so a running emulator is visible even though
the scene itself is static. It also writes a marker into cartridge RAM, so the
test can prove that the CPU, the PPU and the cartridge RAM are all alive.

  --cgb   marks the cartridge as Game Boy Color only (0xC0).
"""
import argparse
import pathlib

LOGO = bytes([
    0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83,
    0x00, 0x0C, 0x00, 0x0D, 0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E,
    0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99, 0xBB, 0xBB, 0x67, 0x63,
    0x6E, 0x0E, 0xEC, 0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E,
])


class Asm:
    """A tiny two pass assembler, enough for the test cartridge."""

    def __init__(self):
        self.code = bytearray()
        self.labels = {}
        self.fixups = []

    def label(self, name):
        self.labels[name] = len(self.code)

    def emit(self, *values):
        self.code += bytes(values)

    def jr(self, name, condition=None):
        # 0x18 = JR r, 0x20 = JR NZ, r, 0x28 = JR Z, r
        opcode = {None: 0x18, "nz": 0x20, "z": 0x28}[condition]
        site = len(self.code)
        self.code += bytes([opcode, 0x00])
        self.fixups.append((site, name))

    def resolve(self):
        for site, name in self.fixups:
            offset = self.labels[name] - (site + 2)
            self.code[site + 1] = offset & 0xFF
        return bytes(self.code)


def build_program(bank_churn=False):
    a = Asm()
    a.emit(0xF3)                      # DI
    # The LCD is switched off while the video memory is written. Real
    # hardware blocks VRAM writes while the PPU is drawing, so a game has to
    # do the same.
    a.emit(0x3E, 0x00)                # LD A, 0x00
    a.emit(0xE0, 0x40)                # LDH (0x40), A   LCD off

    # The cartridge also initializes the CGB palette; BGP is DMG-only.
    a.emit(0x3E, 0x80, 0xE0, 0x68)
    for value in (0xFF, 0x7F, 0xB5, 0x56, 0x4A, 0x29, 0, 0):
        a.emit(0x3E, value, 0xE0, 0x69)

    # Tile 0 is blank, tile 1 is solid. Clear every tile slot first.
    a.emit(0x21, 0x00, 0x80)          # LD HL, 0x8000
    a.emit(0x01, 0x00, 0x04)          # LD BC, 0x0400
    a.label("clear")
    a.emit(0x3E, 0x00)                # LD A, 0x00
    a.emit(0x77)                      # LD (HL), A
    a.emit(0x23)                      # INC HL
    a.emit(0x0B)                      # DEC BC
    a.emit(0x78)                      # LD A, B
    a.emit(0xB1)                      # OR C
    a.jr("clear", "nz")               # JR NZ, clear

    # Tile 1 is solid colour 3. A tile is 16 bytes, so tile 1 follows tile 0.
    a.emit(0x21, 0x10, 0x80)          # LD HL, 0x8010
    a.emit(0x01, 0x40, 0x00)          # LD BC, 0x0040
    a.label("solid")
    a.emit(0x3E, 0xFF)                # LD A, 0xFF
    a.emit(0x77)                      # LD (HL), A
    a.emit(0x23)                      # INC HL
    a.emit(0x0B)                      # DEC BC
    a.emit(0x78)                      # LD A, B
    a.emit(0xB1)                      # OR C
    a.jr("solid", "nz")               # JR NZ, solid

    # The background map alternates between both tiles.
    a.emit(0x21, 0x00, 0x98)          # LD HL, 0x9800
    a.emit(0x01, 0x00, 0x08)          # LD BC, 0x0800
    a.emit(0x16, 0x00)                # LD D, 0x00
    a.label("map")
    a.emit(0x7A)                      # LD A, D
    a.emit(0x77)                      # LD (HL), A
    a.emit(0x23)                      # INC HL
    a.emit(0xEE, 0x01)                # XOR 0x01
    a.emit(0x57)                      # LD D, A
    a.emit(0x0B)                      # DEC BC
    a.emit(0x78)                      # LD A, B
    a.emit(0xB1)                      # OR C
    a.jr("map", "nz")                 # JR NZ, map

    # Clear the object attribute memory, otherwise leftover sprites are drawn
    # on top of the background.
    a.emit(0x21, 0x00, 0xFE)          # LD HL, 0xFE00
    a.emit(0x01, 0xA0, 0x00)          # LD BC, 0x00A0
    a.label("oam")
    a.emit(0x3E, 0x00)                # LD A, 0x00
    a.emit(0x77)                      # LD (HL), A
    a.emit(0x23)                      # INC HL
    a.emit(0x0B)                      # DEC BC
    a.emit(0x78)                      # LD A, B
    a.emit(0xB1)                      # OR C
    a.jr("oam", "nz")                 # JR NZ, oam

    # Switch the LCD on and wait for the first vertical blank.
    a.emit(0x3E, 0x91)                # LD A, 0x91
    a.emit(0xE0, 0x40)                # LDH (0x40), A   LCDC on, background on
    a.emit(0x21, 0x44, 0xFF)          # LD HL, 0xFF44
    a.label("vblank")
    a.emit(0x7E)                      # LD A, (HL)
    a.emit(0xFE, 0x90)                # CP 0x90
    a.jr("vblank", "nz")              # JR NZ, vblank

    # Cartridge RAM marker.
    a.emit(0x3E, 0x1A)                # LD A, 0x1A
    a.emit(0xEA, 0x00, 0x00)          # LD (0x0000), A  cartridge RAM on
    a.emit(0x3E, 0x42)                # LD A, 0x42
    a.emit(0xEA, 0x00, 0xA0)          # LD (0xA000), A
    a.emit(0x3E, 0x43)                # LD A, 0x43
    a.emit(0xEA, 0x01, 0xA0)          # LD (0xA001), A

    # Audible square wave, routed to both stereo outputs (about 512 Hz).
    for register, value in [(0x26,0x80),(0x24,0x77),(0x25,0x11),
                            (0x11,0x80),(0x12,0xF0),(0x13,0x00),(0x14,0x87)]:
        a.emit(0x3E,value)
        a.emit(0xE0,register)

    # The background palette rotates once per frame, so a running emulator
    # stays visible even though the scene itself is static.
    a.emit(0x0E, 0x00)                # LD C, 0x00
    a.label("loop")
    a.emit(0x21, 0x44, 0xFF)          # LD HL, 0xFF44
    a.label("palette")
    a.emit(0x7E)                      # LD A, (HL)
    a.emit(0xFE, 0x90)                # CP 0x90
    a.jr("palette", "nz")             # JR NZ, palette
    a.emit(0x79)                      # LD A, C
    a.emit(0xE6, 0x03)                # AND 0x03
    a.emit(0xF6, 0xE4)                # ORA 0xE4
    a.emit(0xE0, 0x47)                # LDH (0x47), A   BGP
    a.emit(0x0C)                      # INC C
    if bank_churn:
        a.emit(0x79,0xEA,0x00,0x20)    # select MBC5 low bank bits
        a.emit(0x7A,0xE6,0x01,0xEA,0x00,0x30)
        a.emit(0xFA,0x00,0x40,0xEA,0x01,0xC0) # consume bank data
        a.emit(0x79,0xB7)             # wrap low counter -> next high bit
        a.jr("bank_done","nz")
        a.emit(0x14)                  # INC D
        a.label("bank_done")
    a.emit(0xEA, 0x00, 0xC0)          # LD (0xC000), A  scratch in work RAM
    # Leave the vertical blank before waiting for the next one, otherwise the
    # palette would keep changing while the frame is being drawn.
    a.label("leave")
    a.emit(0x7E)                      # LD A, (HL)
    a.emit(0xFE, 0x90)                # CP 0x90
    a.jr("leave", "z")                # JR Z, leave
    a.jr("loop")                      # JR loop
    return a.resolve()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("output")
    parser.add_argument("--cgb", action="store_true",
                        help="mark the cartridge as Game Boy Color only")
    parser.add_argument("--rom-banks",type=int,choices=[2,4,8,16,32,64,128,256,512],default=2)
    parser.add_argument("--bank-churn",action="store_true")
    args = parser.parse_args()

    rom = bytearray(args.rom_banks*16384)
    for bank in range(2,args.rom_banks):rom[bank*16384:(bank+1)*16384]=bytes([bank&255])*16384
    rom[0x104:0x134] = LOGO
    rom[0x134:0x144] = b"PXA CORE TEST\x00\x00\x00"
    rom[0x143] = 0xC0 if args.cgb else 0x00
    rom[0x147] = 0x1b if args.rom_banks>2 else 0x03  # MBC1+RAM+BATTERY
    rom[0x148] = args.rom_banks.bit_length()-2  # 32 KiB
    rom[0x149] = 0x02  # 8 KiB cartridge RAM
    rom[0x14A] = 0x01  # destination code
    rom[0x14B] = 0x33  # old licensee code
    rom[0x14C] = 0x00  # version

    program = build_program(args.bank_churn)
    rom[0x150:0x150 + len(program)] = program

    # Entry point: NOP; JP 0x150
    rom[0x100] = 0x00
    rom[0x101] = 0xC3
    rom[0x102] = 0x50
    rom[0x103] = 0x01

    # Interrupt vectors point at a RETI.
    for vector in (0x40, 0x48, 0x50, 0x58, 0x60):
        rom[vector] = 0xD9

    checksum = 0
    for byte in rom[0x134:0x14D]:
        checksum = (checksum-byte-1)&255
    rom[0x14D] = checksum
    total = sum(rom[:0x14E])+sum(rom[0x150:])
    rom[0x14E:0x150] = (total&65535).to_bytes(2,"big")
    path = pathlib.Path(args.output)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(bytes(rom))
    print(f"wrote {path} ({len(rom)} bytes)")


if __name__ == "__main__":
    main()
