# MSX2-port: plan

Doel: MSX2 (V9938) als tweede machineprofiel, eerst op SDL, daarna Pico
(vereist PSRAM op de WeAct — 128KB VRAM + 128KB mapper-RAM past niet meer
naast de framebuffers in 520KB SRAM).

Bron: de eigen msx_rs-emulator (Rust). Betrouwbaar gebleken eerder bij de
WD2793-port. Locatie: `~/Projects/rust/msx_rs/`.

## Wat waar leeft in msx_rs

- **`src/vdp.rs` (2908 r.)** — CPU-kant van de V9938: 128KB VRAM,
  R0-R46-registeropslag, S0-S9-status, poortprotocol (0x98 data / 0x99
  ctrl+status / 0x9A palette / 0x9B indirect), 17-bit VRAM-adres via R14,
  9-bit palette, command-engine (HMMC/HMMM/HMMV/LMMC/LMCM/LMMM/LMMV/YMMM/
  LINE/SRCH/PSET/POINT — VRAM-effect direct, CE-busy-tijd gemodelleerd à la
  fMSX VdpOpsCnt, 12500/scanline), lijninterrupts (R19/IE1, ack via S1),
  HR-bit-timing (S2 bit 5, 228 T-states/lijn, HBlank vanaf T-state 170).
  LET OP: rendering zit NIET hier maar in de shader.
- **`src/vdp.wgsl` (923 r.)** — de mode-renderers: T1/T2, G1/G2/G3,
  G4 (scr5, 4bpp 256×212), G5 (scr6, 2bpp 512), G6 (scr7, 4bpp 512),
  G7 (scr8, 8bpp), sprite mode 1 én 2 (8/lijn, kleur-per-lijn, CC/IC).
  Per-scanline registersnapshots (R0-R8,R10,R11,R23) voor split-screens.
  → naar C-lijnrenderers vertalen.
- **`src/rtc.rs` (152 r.)** — RP-5C01 RTC, poorten 0xB4/0xB5; de echte
  MSX2-BIOS bewaart bootinstellingen in de CMOS-banken.
- **`src/bus.rs`** — NMS-8245-slotlayout (de ROM-set hebben we al in
  `~/Projects/rust/msx_rs/assets/NMS8245/`):
  - Slot 0: MSX2.ROM (BIOS+BASIC 2.1, 32KB op 0x0000)
  - Slot 1: cartridge
  - Slot 2: FM-PAC/MSX-MUSIC (optioneel, YM2413 op 0x7C/0x7D)
  - Slot 3 (expanded): 3-0 MSX2EXT.ROM (sub-ROM), 3-2 RAM-mapper
    (poorten 0xFC-0xFF, 128/256KB), 3-3 DISK.ROM + WD2793

## Portfases (taken 8-11)

1. **v9938.c core**: VRAM/registers/status/palette/poortprotocol +
   legacy-modes (T1/G1/G2) door de bestaande tms9918-renderers heen.
   Mijlpaal: NMS8245-BIOS boot naar BASIC op SDL.
2. **Bitmap-modes + sprite mode 2** uit vdp.wgsl. Mijlpaal: screen 5-games.
3. **Command-engine** uit vdp.rs (veel MSX2-games zijn er afhankelijk van).
4. **Machine**: mapper-RAM, sub-ROM, RTC, lijn-granulaire emulatielus
   (228 T-states/lijn i.p.v. frame-in-één-keer — nodig voor lijn-IRQs),
   machineprofiel-keuze (MSX1/MSX2 o.b.v. system/-inhoud), menu.

## Architectuurbeslissingen

- `emu/tms9918.c` blijft ONaangeraakt voor het MSX1-profiel; `emu/v9938.c`
  is een apart device met een vergelijkbare context+snapshot-API.
- Emulatielus wordt per-scanline (228 T-states); MSX1-profiel mag op de
  oude frame-lus blijven.
- Output wordt breedte-flexibel: 256 óf 512 pixels breed, 212 lijnen
  (HDMI-kant: 640×480 window, 512→640 met borders; Pico pas in fase 2).
- Per-scanline registersnapshots zoals msx_rs (nodig voor split-scroll,
  Vampire Killer/KV2-achtige effecten); VRAM-snapshot aan het einde van
  de zichtbare scan (niet post-ISR — zie vdp.rs-commentaar bij
  `vram_display` voor het waarom).

## Bekende afhankelijkheden/known issues

- Expanded-slot disk (3-3) ontspoorde met de MSX1-BIOS; msx_rs draait
  disk 3-3 prima met de MSX2-BIOS. Bij fase 4 hertesten; mogelijk lost
  het zichzelf op, anders alsnog de inter-slot-bug vinden.
- Flat-RAM DOS-bootfase (zie machine.c-commentaar) — relevant zodra
  Nextor/DOS2 in beeld komt.
- PSRAM-loading op de WeAct is een voorwaarde voor MSX2-op-Pico.
