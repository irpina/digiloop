# MIDI LOOPBACK, Digitone mk1

An internal MIDI cable for the Digitone mk1 and Digitone Keys: what a MIDI
track sends goes back into the unit instead of out, as if it came in on DIN.
So a MIDI track can sequence and p-lock the Digitone's own tracks and its
master effects: set its CHAN to a synth track's channel or the FX channel
(MIDI CONFIG > CHANNELS) and its CCs to the parameters' CC numbers.

- **LOOP** is knob E on the MIDI tracks' TRIG page (one of its empty boxes):
  OFF or ON, per MIDI track and per pattern.
- **The LOOPBACK tile** in the Mod Menu (hold a track key) opens the same four
  switches, MIDI 1-4; YES on one flips it, and the corner shows M1 LOOP ON.
- A looped message is applied from the UI task, up to a frame (33 ms) after
  the step. A MIDI track with LOOP OFF, or with its CHAN off, sends as
  before.

For OS 1.43 and 1.44, on [core-dn1](https://github.com/irpina/elekloader/tree/main/mods/core-dn1) 5.1. It names no firmware address: its
1.44 port is empty, and both builds carry the same code.

## Install

Not in elekloader's shop yet. You need:
- **core-dn1 5.1,** which is not released yet: build it from elekloader's source
  (`mods/core-dn1`, its README says how) with the stock OS file. Older cores refuse this mod.
- **elekloader from source with core-dn1 5.1** (its `digitone-mk1/core3.h` declares what this
  mod calls), with Python 3.9 or newer, to build the mod and the OS.
- **This mod:** `loopback-0.2.elemod` for OS 1.43, `loopback-0.2-os1.44.elemod` for 1.44, from
  [this repository's releases](https://github.com/irpina/digiloop/releases) once there are any, or built
  from this folder (Build, below).
- **The stock OS file:** `Digitone_and_Digitone_Keys_OS1.44.syx` or `..._OS1.43.syx`, from
  [Elektron's Digitone downloads](https://www.elektron.se/support-downloads/digitone).
  elekloader recognises the file by its hash.

Build your OS (or in elekloader's window, run from source: **+ Install from file...** both files):

```bash
python -m elekloader.patch --stock Digitone_and_Digitone_Keys_OS1.44.syx \
    --mod core-5.1-os1.44.elemod --mod loopback-0.2-os1.44.elemod \
    --out Digitone_OS1.44-loopback.syx --version XXXX
```

`--version` is the 4 characters the unit will show. Flash the `.syx` with
Elektron Transfer, as for any OS update: connect the unit over USB, select
it in Transfer and **Connect**, drag the `.syx` onto **Drop files here**,
and press **YES** on the unit. Don't turn it off until the upgrade is done.

**Recovery:** elekloader never changes the bootloader, so the stock OS
file always restores the unit. Hold **FUNC** while powering on for the
startup menu, and press **TRIG 4** for OS UPGRADE. Then send the stock
`.syx` with Transfer.

## How

The MIDI tracks' messages leave through the sequencer's MIDI engine, which
writes them straight to DIN and USB: they never pass the port switch that
`ev_midi_out` sees. Core 5.0.1's `ev_seq_midi` gives each whole message as the
engine queues it. A channel message on the channel of a MIDI track whose LOOP
is ON (its CHAN: the track's kit sound byte +0xc0, with bit 0 of +0x141 for
on) is taken there and kept in a ring; the UI task's next tick plays it into
the unit with `core_midi_receive`, so the stock applies it by MIDI CONFIG's
channels. A message the stock sends while it applies a looped one goes out,
not round again. LOOP is a lane parameter with no p-locks (core-dn1 5.1's lane parameters (elekloader's docs/ADAPTING.md, "Lane parameters, knobs on pages and TRIG's second page")), on the
MIDI TRIG page through `core_knobs`.

## Build

```bash
python -m elekloader.sdk.build . --stock Digitone_and_Digitone_Keys_OS1.43.syx   # out/loopback-0.2.elemod
python -m elekloader.sdk.build . --stock Digitone_and_Digitone_Keys_OS1.44.syx   # out/loopback-0.2-os1.44.elemod
python -m elekloader.lint out/loopback-0.2.elemod --stock Digitone_and_Digitone_Keys_OS1.43.syx --with core-5.1.elemod
```

## What was checked

In digikit's emulator (digiemu, headless `Session`, the MIDI out captured),
with core-dn1 5.1 and a probe mod, on 1.43 and on 1.44, with MIDI CONFIG's
OUTPUT TO set to MIDI+USB and MIDI track 1's CHAN on (channel 1, T1's):

- The MIDI TRIG page showed LOOP on knob E (ON after a turn).
- LOOP OFF: MIDI track 1's trig went out on DIN, 90 3C 64 then 90 3C 00.
- LOOP ON from the knob: nothing went out, and T1 played the note (a voice
  with no step: it came in as MIDI). Back OFF, then ON from the Mod Menu's
  MIDI 1 switch: the same.
- CHAN found by turning it on the MIDI track's SRC page: the kit sound's
  byte +0xc0 went 0 to 1, and FUNC + its knob set bit 0 of +0x141.

`lint` says it patches no site and links with core-dn1 5.1 on 1.43 and 1.44.

Not checked: CCs and the master effects through the loop (0.1 checked a note
the same way), a CHAN p-lock on a step, USB out, the Digitone Keys, and
anything on hardware.

## Licence

GPL-2.0-or-later, as elekloader's core it builds on (LICENSE).
