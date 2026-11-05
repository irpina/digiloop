# MIDI LOOPBACK, Digitone mk1

An internal MIDI cable for the Digitone mk1 and Digitone Keys: what the
MIDI tracks send on one channel goes back into the unit instead of out, as
if it had come in on DIN. A MIDI track set to that channel then sequences
and p-locks the Digitone itself:

- set the loop channel to a synth track's channel (MIDI CONFIG > CHANNELS,
  1-4 by default) and the MIDI track's notes play that track, its CCs turn
  that track's parameters;
- set it to the FX channel (9 by default) and the MIDI track's CCs turn the
  master effects (chorus, delay, reverb, ...), which a MIDI track can then
  p-lock per step.

The CC numbers are the Digitone's MIDI implementation's (the manual's
appendix). **LOOPBACK** in the Mod Menu (hold a track key) picks the
channel: OFF, the stock, or CH 1-16. It is saved with the project, and the
corner shows LOOP CH n for a second. A message on the loop channel is not
sent out; the other channels go out as ever.

For OS 1.43 and 1.44, on [core-dn1](https://github.com/irpina/elekloader/tree/main/mods/core-dn1) 5.0.1. It names no
firmware address: its 1.44 port is empty, and both builds carry the same
code.

## Install

Not in elekloader's shop yet. You need:
- **core-dn1 5.0.1,** which is not released yet: build it from elekloader's source
  (`mods/core-dn1`, its README says how) with the stock OS file. Older cores refuse this mod.
- **elekloader from source with core-dn1 5.0.1** (its `digitone-mk1/core3.h` declares what this
  mod calls), with Python 3.9 or newer, to build the mod and the OS.
- **This mod:** `loopback-0.1.elemod` for OS 1.43, `loopback-0.1-os1.44.elemod` for 1.44, from
  [this repository's releases](https://github.com/irpina/digiloop/releases) once there are any, or built
  from this folder (Build, below).
- **The stock OS file:** `Digitone_and_Digitone_Keys_OS1.44.syx` or `..._OS1.43.syx`, from
  [Elektron's Digitone downloads](https://www.elektron.se/support-downloads/digitone).
  elekloader recognises the file by its hash.

Build your OS (or in elekloader's window, run from source: **+ Install from file...** both files):

```bash
python -m elekloader.patch --stock Digitone_and_Digitone_Keys_OS1.44.syx \
    --mod core-5.0.1-os1.44.elemod --mod loopback-0.1-os1.44.elemod \
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

The MIDI tracks do not send through the port switch that core's
`ev_midi_out` sees. Their messages go through the sequencer's MIDI engine,
which appends each whole message to one of two buffers and writes those
straight to DIN (0x40002a3a) and USB (0x40004bcc) from 0x400f355a: these are
among the "direct sends" the core-dn1 5.0 hook map counted, and the ones a
MIDI track makes. Core 5.0.1 adds `ev_seq_midi` at the two append routines.
The mod takes a channel message there when it is on the loop channel, keeps
it in a ring (128 messages), and the UI task's next tick (30 a second) plays
each into the unit with core 5.0.1's `core_midi_receive`: the MIDI task's
own handler for its status, as for a message in from DIN, so `ev_midi_cc`
and `ev_midi_pc` see it and the stock applies it by MIDI CONFIG's channels.
MIDI CONFIG's INPUT FROM is not asked (the message did not come in on a
port).

So a looped message lands up to one UI frame (about 33 ms) after its step,
and in the UI task rather than the MIDI task. A message the stock sends
while it applies a looped one goes out, not round again.

## Build

```bash
python -m elekloader.sdk.build . --stock Digitone_and_Digitone_Keys_OS1.43.syx   # out/loopback-0.1.elemod
python -m elekloader.sdk.build . --stock Digitone_and_Digitone_Keys_OS1.44.syx   # out/loopback-0.1-os1.44.elemod
python -m elekloader.lint out/loopback-0.1.elemod --stock Digitone_and_Digitone_Keys_OS1.43.syx --with core-5.0.1.elemod
```

## What was checked

In digikit's emulator (digiemu, headless `Session`, with digiemu's DIN
output captured), with core-dn1 5.0.1, SOLO, PC SOUND and a probe mod
logging `ev_seq_midi` and each voice start, on 1.43 and on 1.44. MIDI
CONFIG's OUTPUT TO set to MIDI+USB, MIDI track 1's channel turned on (1)
and a trig on its step 1, then PLAY:

- LOOPBACK on CH 1: the track's note on and off (90 3C 64, 90 3C 00) came to
  `ev_seq_midi` and nothing went out on DIN; T1, on channel 1, started a
  voice at note 0x3C right after, besides its own pattern's notes. The
  corner showed LOOP CH 1.
- LOOPBACK OFF: the same note went out on DIN (90 3C 64 90 3C 00), and T1
  played only its pattern.
- `core_midi_receive` (core 5.0.1, on 1.43): a CC on channel 1 reached
  `ev_midi_cc` as track 0, one on channel 9 as track 8, the FX.

`lint` with core-dn1 5.0.1 (`--with core-5.0.1.elemod`) says it patches no site and links, on
1.43 and 1.44, with the same code on both; with core-dn1 5.0 it is refused: it needs 5.0.1.

Not checked: a MIDI track's CCs going round (its CC page was not set up in
the emulator; the CC path was checked with `core_midi_receive` alone), a
master effect heard changing, the timing under load, USB output, the
Digitone Keys, and anything on hardware.

## Licence

GPL-2.0-or-later, as elekloader's core it builds on (LICENSE).
