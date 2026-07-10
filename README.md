# PicoCalcMSX

An MSX1 emulator for the [ClockworkPi PicoCalc](https://www.clockworkpi.com/picocalc),
running on a **Raspberry Pi Pico 2 W** (RP2350).

It boots MSX BASIC and runs Konami SCC cartridge games with sound, at 60 Hz.

## Features

- **Emulation** — Z80 (Zeta core) + TMS9918 VDP, 8255 PPI keyboard, Konami SCC mapper.
- **Dual-core** — core 0 runs the emulation paced to 60 Hz; core 1 blits the screen
  in parallel, so the machine stays full-speed while the display refreshes at ~30 fps.
- **Video** — per-scanline rendering straight to the ILI9488 LCD over SPI at 75 MHz
  via DMA (no full framebuffer needed). Toggle between 1:1 and scaled with **Ctrl+Alt**.
- **Audio** — PSG (AY-3-8910) via [emu2149](https://github.com/digital-sound-antiques/emu2149)
  and an integer Konami SCC, mixed out over the PicoCalc's PWM audio (GP26/27).
- **Keyboard** — the PicoCalc's STM32 keyboard over I²C, mapped to the MSX matrix.

Status: **work in progress**, MSX1 only.

## Hardware

- Raspberry Pi Pico 2 W (RP2350) in a PicoCalc.
- Flash via a Raspberry Pi Debug Probe (SWD), or drag-and-drop the `.uf2` in BOOTSEL mode.

## Building

Requires the [Pico SDK](https://github.com/raspberrypi/pico-sdk) (2.x) and the ARM toolchain.

```sh
# 1. Fetch the Zeta Z80 core (not bundled — see Credits)
sh tools/setup-zeta.sh

# 2. Supply your own ROMs and generate the headers (see below)

# 3. Build
cmake -B build -G Ninja
ninja -C build
```

The resulting `build/PicoCalcMSX.uf2` can be dropped onto the Pico in BOOTSEL mode.

## ROMs (not included)

No ROMs are distributed with this project. Supply your own and generate C headers:

```sh
# MSX BIOS, padded to 64 KiB:
python3 tools/rom_to_header.py MSX1.rom emu/bios_rom.h bios_rom 65536

# A cartridge (e.g. an SCC game):
python3 tools/rom_to_header.py YOURGAME.ROM emu/game_rom.h game_rom
```

`emu/machine.c` includes the BIOS and one game header — edit the game `#include`
to point at yours. For a fully redistributable build, use the open-source
[C-BIOS](https://cbios.sourceforge.net/) instead of a machine BIOS.

## Credits

- **Zeta Z80 CPU core** — Manuel Sainz de Baranda y Goñi ([redcode/Z80](https://github.com/redcode/Z80)).
- **emu2149** (AY-3-8910/YM2149) — Mitsutaka Okazaki.
- **SCC** sound — integer implementation from my [RogueDrive](https://github.com/) project.
- **PicoCalc** — ClockworkPi.

Third-party components keep their own licenses. My own code is MIT (see LICENSE).
