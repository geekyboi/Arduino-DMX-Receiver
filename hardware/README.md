# Hardware: Arduino Nano DMX Shield

A carrier board for an Arduino Nano that receives DMX512 and turns four DMX channels into four PWM outputs. The board was designed in EasyEDA (Standard) and has gone through four revisions.

- **EasyEDA project:** [Arduino Nano DMX Spinner](https://easyeda.com/oliversherratt/Arduino_Nano_DMX_Spinner-b9e2eea62d494f0aa066720658f32e5c) (MIT)
- **Current revision:** PCB V4, silkscreen "Arduino Nano DMX Shield, Version 1.0"
- **Board size:** about 72 × 109 mm, 2 layers, GND pour on both sides

## Block diagram

```
 XLR IN  ──┐                         ┌── OUT1 (D5)  ─ 2N3904 ─ P1
 XLR THRU ─┼─ D+/D- ─ MAX481 ─ RX/TX ┤── OUT2 (D9)  ─ 2N3904 ─ P2
 3-pin hdr ┘          (DE/RE ← D3)   ├── OUT3 (D10) ─ 2N3904 ─ P3
                                     └── OUT4 (D11) ─ 2N3904 ─ P4
                       Arduino Nano
              UP (A0) ─┤           ├─ SDA (A4) ─┐
            DOWN (A1) ─┤           ├─ SCL (A5) ─┴─ 0.96" SSD1306 OLED
```

## Arduino Nano pin map

| Nano pin | Net | Used for |
|---|---|---|
| D0 / RX | DMX_RX | MAX481 RO (pin 1) |
| D1 / TX | DMX_TX | MAX481 DI (pin 4) |
| D3 | DMX_ENABLE | MAX481 RE and DE (pins 2 + 3), low = receive |
| D5 | OUTPUT1 | Q1 base (PWM, Timer0, about 980 Hz) |
| D9 | OUTPUT2 | Q2 base (PWM, Timer1, about 490 Hz) |
| D10 | OUTPUT3 | Q3 base (PWM, Timer1, about 490 Hz) |
| D11 | OUTPUT4 | Q4 base (PWM, Timer2, about 490 Hz) |
| A0 | BUTTON1 | UP button S1 to GND (internal pull-up) |
| A1 | BUTTON2 | DOWN button S2 to GND (internal pull-up) |
| A4 | SDA | OLED D1 |
| A5 | SCL | OLED D1 |
| 5V | +5V | MAX481, OLED, output jumpers |
| VIN | VCC | Power input (see below) |
| GND | GND | |

The Nano plugs into two 15-way female headers (P8, P10), so it can be lifted out. That matters, because **you have to remove the Nano to upload firmware**: the MAX481 drives the Nano's RX line and beats the USB-serial chip.

## DMX connectors

U3 and U6 are Neutrik NC3 chassis connectors (reverse-mount footprint, `NC3FAH1-D`), and U4 is a 3-pin header for a flying lead. All three are wired in parallel, so any one can be IN and another THRU.

| XLR pin | Signal | Wire colour used |
|---|---|---|
| 1 | GND / common | screen |
| 2 | Data − | red |
| 3 | Data + | white |

There is no on-board 120 Ω terminator. If this receiver is the last device on the line, plug a DMX terminator into the spare connector.

## Output stage and solder jumpers

Each output has a 2N3904 NPN transistor, a 2-pin header (P1 to P4) and two solder jumpers. Close **one** jumper per channel, depending on what you are driving:

| Mode | Close | P*n* pin 1 | P*n* pin 2 | Use for |
|---|---|---|---|---|
| Low-side switch (open collector) | **SJG*n*** (emitter to GND) | switched output (collector) | GND | LEDs or relays powered from their own supply, up to 200 mA and 40 V (2N3904 limits) |
| 5 V source (emitter follower) | **SJV*n*** (collector to +5V) | +5V | output (emitter, about 4.3 V at full) | PWM or logic inputs on motor or LED drivers |

Never close both jumpers on the same channel: that shorts the transistor straight across 5 V. Keep loads on the 5 V source mode small, because they come out of the Nano's regulator.

## Display and buttons

- **D1:** 0.96" 128×64 I²C OLED (SSD1306, 4-pin GND/VCC/SCL/SDA), address 0x3C.
- **S1 / S2:** UP / DOWN 6 mm tactile switches to GND. P5 and P6 break the two button lines out to single pins.
- **Breakaway front panel:** the bottom section of the board carries a second OLED footprint (D2) and a second pair of buttons (S3 UP, S4 DOWN) on a 6-pin header P7. Nothing on the PCB connects it to the Nano; you wire it by hand when you want the display and buttons on a front panel.

  | P7 pin | Connect to |
  |---|---|
  | 1 | DOWN (A1, or P6) |
  | 2 | SDA (A4) |
  | 3 | SCL (A5) |
  | 4 | +5V |
  | 5 | GND |
  | 6 | UP (A0, or P5) |

## Power

The VIN pin of the Nano is brought out as **VCC** next to GND on the silkscreen. Feed it 7–12 V DC, or power the Nano over USB for bench testing. V2 and V3 had a separate power connector; V4 removed it.

## Bill of materials (V4)

| Ref | Qty | Part | Package | Notes |
|---|---|---|---|---|
| U1 | 1 | Arduino Nano (ATmega328P) | Nano module | Fits in P8 + P10 |
| P8, P10 | 2 | Female header 1×15, 2.54 mm | THT | LCSC C54104 |
| U2 | 1 | MAX481CPA RS-485 transceiver | DIP-8 | MAX485 or SN75176 are pin compatible |
| U3, U6 | 2 | Neutrik NC3 chassis XLR (male and/or female) | Reverse mount | e.g. NC3FAH1 (female), NC3MAH (male) |
| U4 | 1 | 3-pin header | 1×3, 2.54 mm | Optional DMX flying lead |
| Q1–Q4 | 4 | 2N3904 NPN | TO-92 | LCSC C2081 |
| P1–P4 | 4 | Header 1×2, 2.54 mm | THT | Outputs; LCSC C49661 |
| SJV1–4, SJG1–4 | 8 | Solder jumpers | PCB | Close one per channel |
| S1–S4 | 4 | Tactile switch 6×6 mm | THT | S3/S4 only for the front panel |
| D1, D2 | 2 | 0.96" I²C OLED SSD1306 | 4-pin module | D2 only for the front panel |
| P5, P6 | 2 | Header 1×1 | THT | Button break-outs |
| P7 | 1 | Female header 1×6 | THT | Front panel link; LCSC C40877 |

A CSV copy is in [`bom.csv`](bom.csv).

## Revision history

| Revision | Changes |
|---|---|
| V1 | Single output transistor, two buttons with external pull-down resistors (R1, R2), XLR in/out |
| V2 | Four transistor outputs with SJV/SJG mode jumpers, external power connector, OLED |
| V3 | Second XLR footprint (U6) and layout rework |
| V4 | Button resistors removed (uses the Nano's internal pull-ups, buttons switch to GND), power connector removed (uses VIN), breakaway front panel with second OLED and buttons |

## Known issues and ideas for a V5

1. **No base resistors on Q1–Q4.** The Arduino pins drive the 2N3904 bases directly. In low-side mode the base-emitter junction is a diode to ground, so each pin sources as much current as it can: around 40 mA or more, which is at or above the ATmega328P's absolute maximum per pin. Add a 1 kΩ resistor in series with each base. On existing boards, you can cut the base trace and bridge it with a resistor.
2. **No DMX termination option.** Add a 120 Ω resistor with a solder jumper across D+/D−.
3. **Firmware upload needs the Nano removed.** Add a jumper or a 1 kΩ resistor in series with MAX481 RO, so the USB-serial chip can win during uploads.
4. **No input protection on the DMX lines.** A TVS diode array (e.g. SM712) across D+/D− is cheap insurance on a stage.
5. **5 V source mode is current-limited** by the Nano's regulator. For heavier loads, switch to logic-level MOSFETs.
