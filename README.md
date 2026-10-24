# Arduino DMX Receiver

A 4-channel DMX512 receiver built on an Arduino Nano. It takes four consecutive DMX channels and turns them into four PWM outputs. You set the DMX address with two buttons and an OLED display, and it keeps its settings after power-off.

I originally built it to put a cheap disco "spinner" light under DMX control, but the outputs work for anything that takes PWM or needs a switched low side: LED strips through a driver, motor speed controllers, relays.

## Features

- Receives standard DMX512 through a MAX481 RS-485 transceiver, with XLR in and through
- 4 PWM outputs on consecutive channels (address, address+1, address+2, address+3)
- Each output can be a low-side switch or a 5 V source, chosen with solder jumpers
- Address from 1 to 509, set with UP/DOWN buttons and shown on a 0.96" OLED
- Per-output min and max levels, so you can limit or invert an output
- Choose what happens when the DMX signal is lost: go to zero, or hold the last values
- Settings saved to EEPROM, checked on boot, and written only once you stop pressing buttons
- Optional breakaway front panel with its own OLED and buttons

## Repository layout

```
firmware/
  DMX_Receiver/          current firmware for the V4 shield
  legacy/DMX_Spinner_RECV/  original 2018 single-output sketch, kept for reference
hardware/
  README.md              pinout, jumper settings, BOM, revision history, known issues
  bom.csv
```

The PCB design lives in EasyEDA: [Arduino Nano DMX Spinner](https://easyeda.com/oliversherratt/Arduino_Nano_DMX_Spinner-b9e2eea62d494f0aa066720658f32e5c).

## Quick start

1. **Build the board.** See [hardware/README.md](hardware/README.md) for the BOM. For each output you use, close **either** SJG (low-side switch) **or** SJV (5 V source), never both.
2. **Install the libraries** from the Arduino Library Manager:
   - DMXSerial (Matthias Hertel)
   - OneButton (Matthias Hertel)
   - SSD1306Ascii (Bill Greiman)
3. **Upload** `firmware/DMX_Receiver/DMX_Receiver.ino` with the board set to *Arduino Nano* (*ATmega328P*, or *ATmega328P (Old Bootloader)* for most clones). Lift the Nano out of the shield first: the DMX transceiver holds the RX pin and blocks uploads.
4. **Plug the Nano back in**, power it through VIN (7–12 V) or USB, and connect DMX.

## Using it

The home screen shows the address range, the signal status and the four output levels:

```
DMX ADDRESS
  001-004
* SIGNAL OK

O1 255  O2 000
O3 128  O4 064
```

| Screen | Button | Action |
|---|---|---|
| Home | UP / DOWN click | Address +1 / −1 |
| Home | UP / DOWN hold | Scroll the address, faster the longer you hold |
| Home | DOWN double-click | Open settings |
| Settings | DOWN click | Next setting |
| Settings | UP click | Value +1 |
| Settings | UP / DOWN hold | Scroll the value |
| Settings | DOWN double-click | Back to home |

**Settings**

| Setting | Default | What it does |
|---|---|---|
| Out*n* min | 0 | Output level when the channel is at 0 |
| Out*n* max | 255 | Output level when the channel is at 255. Set it below min to invert the output. |
| No signal | zero | `zero`: outputs act as if every channel were 0. `hold`: keep the last values. |
| Factory reset | | UP once to arm, UP again to confirm |

While you edit a min or max value, that output shows the value live, so you can see the limit on the fixture itself. The screen switches off after 30 seconds without a button press; any button wakes it. Settings leaves itself after 30 seconds.

## Wiring the outputs

| Jumper | P*n* pin 1 | P*n* pin 2 | Example |
|---|---|---|---|
| SJG*n* closed | load − (switched) | GND | 12 V LED strip: strip + to 12 V, strip − to pin 1, PSU − to pin 2 |
| SJV*n* closed | +5V | PWM out | PWM input on a motor or LED driver |

The 2N3904 handles up to 200 mA, so for bigger loads drive a MOSFET or a driver board from the output. See the [known issues](hardware/README.md#known-issues-and-ideas-for-a-v5) before connecting heavy loads.

## Licence

MIT. See [LICENSE](LICENSE).
