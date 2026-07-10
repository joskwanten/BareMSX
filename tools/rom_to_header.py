#!/usr/bin/env python3
"""Convert a binary ROM into a C header with a const uint8_t array (in flash).

Usage:
    python3 tools/rom_to_header.py <rom-file> <output.h> <array_name> [pad_size]

Examples:
    # MSX BIOS, padded to 64 KiB (BIOS reads may go above 32 KiB):
    python3 tools/rom_to_header.py MSX1.rom emu/bios_rom.h bios_rom 65536

    # A cartridge ROM (no padding):
    python3 tools/rom_to_header.py SALAMAND.ROM emu/salamander.h game_rom

The generated array lives in .rodata (flash/XIP on the RP2350).
Supply your own ROMs — they are not distributed with this project.
"""
import sys


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        sys.exit(1)

    src, dst, name = sys.argv[1], sys.argv[2], sys.argv[3]
    pad = int(sys.argv[4]) if len(sys.argv) > 4 else 0

    data = bytearray(open(src, "rb").read())
    raw = len(data)
    if pad and pad > len(data):
        data += bytes(pad - len(data))

    guard = name.upper() + "_H"
    with open(dst, "w") as f:
        f.write(f"#ifndef {guard}\n#define {guard}\n#include <stdint.h>\n\n")
        f.write(f"// Generated from {src} ({raw} bytes"
                + (f", padded to {len(data)}" if pad else "") + ").\n")
        f.write(f"#define {name.upper()}_SIZE {len(data)}\n\n")
        f.write(f"static const uint8_t {name}[{name.upper()}_SIZE] = {{\n")
        for i in range(0, len(data), 16):
            f.write("    " + ", ".join(f"0x{b:02x}" for b in data[i:i + 16]) + ",\n")
        f.write(f"}};\n\n#endif // {guard}\n")

    print(f"Wrote {dst}: {len(data)} bytes as '{name}'")


if __name__ == "__main__":
    main()
