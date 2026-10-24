# V5 PCB plan

Changes planned for the next board revision. Each one fixes a problem found on V1–V4 (see [Known issues](README.md#known-issues-and-ideas-for-a-v5)). Part numbers are suggestions; anything equivalent is fine.

## 1. Outputs: MOSFETs instead of 2N3904

The 2N3904s are limited to 200 mA and several burned out on the original build. Replace each one with a logic-level N-channel MOSFET as a low-side switch.

| Per output | Part | Why |
|---|---|---|
| Q*n* | AO3400A (SOT-23, ~5 A) or IRLZ44N (TO-220, for bigger loads) | Fully on at 5 V gate drive, almost no heat at a few amps |
| Gate resistor | 100 Ω, Arduino pin → gate | Limits the pin current when the gate charges |
| Gate pull-down | 10 kΩ, gate → GND | Keeps the output off while the Nano boots or is unplugged |
| Flyback diode | SS34 (or 1N5819), across the load terminals | Protects the MOSFET from motors, relays and solenoids. Fit it only when needed |
| Connector | 2-pin 5.08 mm screw terminal: LOAD+ (from VIN) and LOAD− (drain) | Rated for real current. Thick traces (≥ 2 mm) from the terminal to the MOSFET and GND |

This drops the SJV (5 V source) mode. If a 5 V PWM logic output is still wanted, add a separate pin header straight from each Arduino pin through a 220 Ω resistor.

## 2. Power input

| Part | Why |
|---|---|
| DC barrel jack (5.5 × 2.1 mm) and/or 2-pin screw terminal | Proper 12 V input again (V4 only has the VIN pad) |
| Polyfuse or blade fuse sized to the total load | Protects the supply and the traces |
| P-channel MOSFET or Schottky diode in series | Reverse-polarity protection |
| Small 5 V buck module (e.g. MP1584 / Mini-360) feeding the 5V net | Stops the Nano's linear regulator overheating at 12 V |
| 100 µF bulk capacitor at the input, 10 µF + 100 nF on 5 V | Rides out switching spikes from the loads |

## 3. DMX input

| Part | Why |
|---|---|
| 120 Ω resistor + solder jumper across D+/D− | Termination when this is the last fixture on the line |
| SM712 TVS diode across D+/D−/GND | Survives static and miswired cables |
| 1 kΩ resistor between MAX481 RO and Nano RX | USB-serial chip wins during uploads, so no need to lift the Nano |
| Optional: isolated transceiver (e.g. ISO1500 or MAX485 + 6N137 + B0505S) | Breaks ground loops on a real stage. Bigger change, so only if wanted |

## 4. Housekeeping

- 4 × M3 mounting holes in the corners, plus 2 on the breakaway front panel.
- On-board link to the front panel: a 6-pin header and ribbon cable, or V-cut traces, so P7 doesn't need hand wiring.
- Silkscreen showing the real revision ("V5") and the date, plus labels on every terminal (LOAD+, LOAD−, 12V, GND, D+, D−).
- Test points for 5V, GND, DMX RX and each gate.
- Keep the Nano pin map unchanged (D3, D5, D9, D10, D11, A0, A1, A4, A5), so the current firmware runs on V5 without changes.
