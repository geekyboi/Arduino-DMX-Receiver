# Handover: Arduino DMX Receiver

Context for picking this project up in a new Claude conversation. Written 24 October 2026.

## The project

A 4-channel DMX512 receiver shield for an Arduino Nano, by Oliver Sherratt (GitHub: geekyboi). Four consecutive DMX channels drive four PWM outputs through 2N3904 transistors. The DMX address is set with UP/DOWN buttons and shown on a 0.96" OLED. It was first built (2017–18) to put a disco "spinner" light on DMX.

## Where everything is

| What | Where |
|---|---|
| GitHub repo | https://github.com/geekyboi/Arduino-DMX-Receiver (public, MIT). `main` still has only the 2017 two-line README. |
| Work in progress | Branch `flesh-out-v4`, one commit `2b22c40`, **not pushed**. It's in `Arduino-DMX-Receiver.bundle` in this folder (see "Restoring the git branch"). |
| This folder on the Mac | `~/Documents/Projects/Arduino-DMX-Receiver/` |
| PCB design | EasyEDA Standard project "Arduino Nano DMX Spinner": https://easyeda.com/oliversherratt/Arduino_Nano_DMX_Spinner-b9e2eea62d494f0aa066720658f32e5c. Open it in the editor while signed in as oliversherratt; the public page doesn't render for bots. |
| EasyEDA document IDs | schematic `93b8cca89c204846a0d65c6b7890ed52` · PCB V1 `73708b5a127f490eb7bf71c66bcbae0a` · V2 `64799b3027774a75b7bfe0de4e0b8072` · V3 `63ba030c9d3b4b76855b403df43dea6a` · **V4 (current) `c9b574fc88db44f384b16638bdeb5ea3`** |
| Original sketch | `~/Documents/Arduino/DMX_Spinner_RECV/DMX_Spinner_RECV.ino` (Jan 2018), with Neutrik NC3FAH1 / NC3MAH datasheets alongside |
| Related but separate | `~/Documents/Arduino/DMX_spots/DMX_spots.ino`: a DMX *transmitter* (ArduinoDMX), not this project |

There was never a DIP / piano-switch version; the old README's mention of one was removed.

## Hardware facts (from the V4 netlist)

- **Nano pins:** D0/D1 = MAX481 RO/DI · D3 = MAX481 DE+RE · outputs D5, D9, D10, D11 · UP A0, DOWN A1 (to GND, no resistors on V4) · OLED SDA A4 / SCL A5 · VIN = "VCC" net.
- **Outputs:** each has a 2N3904 (pin 1 E, 2 B, 3 C) and a 2-pin header P*n* (pin 1 collector, pin 2 emitter).
  - SJG*n* ties the emitter to GND, giving a low-side switch.
  - SJV*n* ties the collector to +5V, giving an emitter-follower 5 V source.
  - Close only one per channel.
- **DMX:** U3 and U6 are NC3 XLRs, U4 is a 3-pin header, all in parallel. Pin 1 GND, 2 D− (red), 3 D+ (white). No termination.
- **Breakaway front panel:** a second OLED (D2) and buttons (S3, S4) on P7. P7 pinout: 1 DOWN, 2 SDA, 3 SCL, 4 +5V, 5 GND, 6 UP. It is not connected to the Nano on the PCB.
- **Board:** about 72 × 109 mm, 2 layers, silkscreen "Arduino Nano DMX Shield, Version 1.0".
- **Revisions:**
  - V1: 1 output, button pull-downs
  - V2: 4 outputs, jumpers, power connector, OLED
  - V3: adds U6
  - V4: removes the button resistors and power connector, adds the front panel

## Firmware status (`firmware/DMX_Receiver/DMX_Receiver.ino`)

- **Libraries:** DMXSerial, OneButton, SSD1306Ascii (128×64, address 0x3C).
- **Behaviour:**
  - Address 1–509, saved in EEPROM.
  - Per-output min/max (max below min inverts).
  - Signal-loss mode: zero or hold.
  - Factory reset.
  - Screen sleeps at 30 s; settings menu times out at 30 s.
- **EEPROM:** a struct with magic, version and checksum, written with `EEPROM.put` 2 s after the last button press.
- **Buttons:** same as the 2018 sketch. Click or hold changes the address; DOWN double-click toggles settings; in settings, DOWN click = next item.
- **Verified:** syntax-checked with g++ against stub headers, and the `scaleLevel()` mapping exhaustively tested.
- **Not yet done:**
  - **Not yet compiled with avr-gcc** (the toolchain wasn't available in that session).
  - **Not tested on hardware.** First job: build in the Arduino IDE and check RAM/flash.
- **Fixed compared with the 2018 sketch:**
  - D9 used as both light output and OLED reset
  - `a > b > c` range checks
  - the `0 <= setting <= 1` preview test
  - EEPROM written about 50 times a second while a button is held
  - only one output
  - the 64×48 MicroOLED library on a 128×64 screen
- **Changed default:** the old sketch defaulted to inverted output (Start 255, End 0); the new default is 0→255. **Oliver to confirm.**

## Known hardware issues (documented in `hardware/README.md`)

1. No base resistors on Q1–Q4. With the ground jumper (SJG) closed, a GPIO pin can source 40 mA or more. Add 1 kΩ per base.
2. No 120 Ω termination option.
3. Firmware uploads need the Nano lifted out, because the MAX481 RO holds RX. Fix: series resistor or jumper.
4. No TVS protection on the DMX lines.

## Open questions for Oliver

- Is the inverted output wanted by default (as the spinner may need)?
- Is power really VIN 7–12 V on V4? The README says so, but it's assumed from the netlist.
- Is the "disco spinner" origin story in the README right?

## Next steps

1. Oliver exports from EasyEDA (schematic PDF, Gerbers, EasyEDA source JSON) and adds photos. Put them in `hardware/` (e.g. `hardware/schematic.pdf`, `hardware/gerbers/`, `hardware/easyeda/`, `docs/images/`).
2. Compile with arduino-cli (`arduino:avr:nano`) and fix anything that comes up; test on the board.
3. Push `flesh-out-v4` to GitHub (**ask Oliver first**: the repo is public), then open a PR or merge to `main`.
4. Optional: a V5 PCB with base resistors, a termination jumper, an upload-friendly RX, TVS diodes, and MOSFET outputs.

## Restoring the git branch

```bash
git clone Arduino-DMX-Receiver.bundle Arduino-DMX-Receiver
cd Arduino-DMX-Receiver
git checkout flesh-out-v4
git remote set-url origin https://github.com/geekyboi/Arduino-DMX-Receiver.git
```

The loose files in this folder are the same as the branch's contents, for easy reading.
