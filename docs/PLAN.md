# SPENNINGUR — plan

Revision 3. Changes since revision 2 are marked **changed**, **new** or **removed** in the text.

## What it is

A mono-in audio effect. It listens to a single melodic line, works out which note is being played, picks a chord for it from the scale and style the user sets, and rings that chord around the note through a bank of tuned resonators. The input is the excitation, so your playing shapes the attack and tone of the ring.

Over successive notes the chords follow a progression rather than a fixed harmony per pitch. The root can be set by hand or follow the input.

**Removed:** MIDI out. Chords as MIDI will be a separate plug-in later (see "Later: a separate MIDI plug-in"). This plug-in is audio only.

Formats: VST3 first, with AU and CLAP as nice-to-haves (milestone 6). Licence: open source, AGPL-3.0.

## Signal flow

```
IN -> INPUT -> PITCH DETECT -> NOTE TRACKER -> ROOT -> CHORD ENGINE -> RESONATOR BANK -> ADSR -> MIX -> LIM -> OUT
        |                                         ^         ^
        |                                    TRACK / SCALE / CHORD / VOICES / MODE
        +--------------------------------------- dry ---------------------------------------->
```

Only the resonator bank, the envelope and the mix touch audio. Everything left of the bank is control.

## Parameters

| Section | Parameter | Range / options | Default | Control | Colour |
|---|---|---|---|---|---|
| Input | IN | -12 to +12 dB | 0 | Knob sm | coral |
| Input | GATE | -72 to -24 dB | -58 | Knob sm | lilac |
| Input | RANGE | SMALL / MEDIUM / LARGE | MEDIUM | Selector | coral |
| Harmony **changed: key and style are one section** | ROOT | C to B (12) | A | Dropdown | coral, violet while tracking |
| Harmony | TRACK | off / on | off | Latch | violet |
| Harmony | FOLLOWS | NOTE / KEY | KEY | Selector | violet |
| Harmony | SCALE **changed** | Major, Minor, Dorian, Phrygian, Lydian, Mixolydian, Harmonic Minor, Melodic Minor, Pent Major, Pent Minor, Blues | Minor | Dropdown | coral |
| Harmony | CHORD | Triad, 7th, 9th, Sus2, Sus4, Add9, Power, Quartal, Shell, Drone | 7th | Dropdown | coral, violet while RANDOM is on |
| Harmony | RANDOM | off / on | on | Latch | violet |
| Harmony | VOICES | 2 / 3 / 4 / 5 / 6 | 5 | Selector | coral |
| Progression | MODE | Follow / Cadence / Wander | Cadence | Selector | coral |
| Progression | TENSION | 0 to 100% | 35 | Knob lg | coral |
| Progression | HOLD | 50 to 1000 ms | 300 | Knob sm | lilac |
| Resonator | RING | 0.2 to 8 s (T60) | 4.5 | Knob lg | teal |
| Resonator | DAMP | 1 to 12 kHz | 6.5 | Knob md | teal |
| Resonator | SPREAD | 0 to 3 octaves | 1.2 | Knob md | teal |
| Resonator | GLIDE | 0 to 200 ms | 40 | Knob sm | lilac |
| Resonator | ATTACK | 1 ms to 2 s | 30 ms | Fader | teal |
| Resonator | DECAY | 10 ms to 2 s | 413 ms | Fader | teal |
| Resonator | SUSTAIN | 0 to 100% | 60% | Fader | teal |
| Resonator | RELEASE | 20 ms to 8 s | 2 s | Fader | teal |
| Output | MIX | 0 to 100% wet | 60 | Knob xl | steel |
| Output | LIM | off / on | on | Latch | amber |

The MIDI parameters from revision 2 (MIDI, CHANNEL, OCTAVE, LENGTH, VELOCITY, FOLLOW, STRUM) are **removed**.

## How each stage works

### 1. Pitch detection

- **Algorithm:** McLeod Pitch Method (MPM), hop of 256 samples. YIN is the fallback if MPM misbehaves on the test set. Written in-house.
- **RANGE** sets the band searched and the window length. It trades reach for latency:

  | RANGE | Searches about | Window | Notes |
  |---|---|---|---|
  | SMALL | C3 to C5 | 1024 | Lowest latency. Fewest octave errors. Voice, lead synth |
  | MEDIUM | E2 to E6 | 2048 | Guitar, most voices |
  | LARGE | B0 to C7 | 4096 | Bass and wide leaps. Highest latency |

- **Note tracker:** a new note fires when the estimate holds within 35 cents for 3 hops, the clarity is above 0.8 and the signal is above GATE. Hysteresis on both thresholds stops chatter at note tails.
- **Out-of-key notes** snap to the nearest scale degree for chord purposes.
- **Latency:** the chord arrives roughly 25 to 45 ms after the note starts at MEDIUM. The resonators are already excited by then, so it reads as a bloom.

### 2. Root: set by hand or tracked

With TRACK off, ROOT is the dropdown value. With TRACK on, the dropdown locks, shows the live root, and turns violet, because the plug-in is now modulating it. FOLLOWS picks how:

- **NOTE:** the root is the last stable note. The chord is always built on the note you played, and SCALE decides whether it is major, minor and so on. The chord root is then always scale degree 0, so MODE has nothing to choose and is bypassed. The panel dims it.
- **KEY:** the root is the tonal centre. A pitch-class histogram with a fixed half-life of about 12 seconds is scored against the selected scale at all 12 roots, with extra weight on the candidate root itself. A new root only replaces the current one when it wins by 20%, so a passing out-of-key note does not flip the key. MODE works as normal on top of it.

The half-life and the margin are constants for now.

### 3. Chord engine

Pure C++ with no audio dependencies, so it can be unit-tested offline. It is also built as its own static library, `spenningur-core`, so the later MIDI plug-in can reuse the detector, the root tracker and the chord engine without copying them.

**Chord shapes are stacks of scale steps.** A triad is steps `0 2 4` from the chord root, a 7th is `0 2 4 6`, and so on. This works for any scale, including the 5- and 6-note ones, and every chord stays in key by construction.

| Style | Steps | Notes |
|---|---|---|
| Triad | 0 2 4 | |
| 7th | 0 2 4 6 | |
| 9th | 0 2 4 6 8 | |
| Sus2 | 0 1 4 | |
| Sus4 | 0 3 4 | |
| Add9 | 0 2 4 8 | |
| Power | 0 4 | Root and fifth only |
| Quartal | 0 3 6 | Stacked fourths |
| Shell | 0 2 6 | Root, third, seventh |
| Drone | 0 4 octave | Root, fifth, root an octave up |

**VOICES** is how many chord tones sound, from 2 to 6. A style with more tones than VOICES is cut from the top, so the root, third and fifth survive longest. A style with fewer is filled by doubling its tones an octave up.

**Choosing the chord root.** The detected note sits at scale degree `d`. In a 7-note scale it belongs to three diatonic triads, rooted on `d`, on `d-2` (it is the third) or on `d-4` (it is the fifth). MODE picks among them:

- **Follow:** the candidate closest to the previous chord root.
- **Cadence:** prefers root motion a fourth up, a fifth up, or a step up.
- **Wander:** weighted random among the candidates. TENSION widens the pool to the third candidate.

TENSION also biases toward extensions and away from the tonic. **HOLD** quantises chord changes: notes that arrive inside the window do not change the chord.

**RANDOM.** When on, a style is drawn from the ten for every chord change, not every note. The RNG is a xorshift seeded per instance and advanced only on the audio thread, so a render is repeatable given the same input and seed. The CHORD dropdown shows the drawn style live and turns violet. Choosing a style while RANDOM is on sets the style that returns when RANDOM goes off.

### 4. Resonator bank and envelope

- **Voice:** a feedback comb with a fractional delay (Lagrange 3rd order), a one-pole low-pass in the loop (DAMP) and a DC blocker. A Karplus-Strong string driven from outside.
- **Excitation:** the input, high-passed at 80 Hz, scaled by IN.
- **RING** sets the T60. The loop gain is derived per voice from its delay length, so low and high voices decay together. Clamped below 0.9995.
- **Chord changes:** each voice's delay slews to its new target over GLIDE. At 0 ms the voice swaps and crossfades over 20 ms.
- **Gain:** the bank is scaled by 1/sqrt(VOICES), so changing VOICES does not change loudness.
- **Output:** mono in, stereo out. Voices are panned across the field by SPREAD order. The dry signal stays centred.

**ADSR.** The envelope is triggered by the note tracker, once per chord change. It scales the resonator output, and it also drives a small internal excitation (a soft noise burst shaped by the same envelope) into the bank. The second part matters. If the envelope only scaled the output, a plucked input could never become a pad, because nothing would be exciting the resonators once the pluck had gone. With the internal excitation, the SUSTAIN stage keeps the chord sounding for as long as the note is held.

| Sound | ATTACK | DECAY | SUSTAIN | RELEASE |
|---|---|---|---|---|
| Transient | 1 ms | 150 ms | 0% | 200 ms |
| Pad | 600 ms | 400 ms | 80% | 3 s |

These are starting points, not tuned values. The internal excitation is the DSP decision I am least sure of, so it gets prototyped first in milestone 3.

**LIM** is a soft-knee peak limiter on the wet path, with the amber lamp lit while it is reducing gain.

## Later: a separate MIDI plug-in

Out of scope for this plug-in. Notes for when it is picked up, so the finding is not lost:

- It reuses `spenningur-core` for detection, root tracking and chords, and only adds a MIDI engine (note-on at each chord change, note-off at the next one or when the input note ends, velocity, strum, octave, channel).
- **Ableton Live routing is the hard part.** As far as I know, Live does not route MIDI out of a plug-in on an audio track. I could not test this from here, and it varies by Live version. Because the new plug-in is separate, it can be built as an instrument-type plug-in for a MIDI track, which sidesteps the problem for output but not for input: an instrument on a MIDI track has no audio input to detect from.
- The likely answer is a virtual MIDI port, which on macOS a plug-in can create with JUCE's virtual-device support. On Windows it would need a driver such as loopMIDI. The audio would come either from a sidechain or from a small audio-effect half that sends the detected notes to the MIDI half inside the host process.
- That decision needs a short spike in your version of Live before any design. It is parked until then.

## Stack and licence

- **JUCE 8, C++20, CMake**, as in the other Sungam plug-ins, which draw their panels in `Panel.cpp` and `Panel.h`.
- **Formats:** VST3 first. AU is built natively by JUCE and is cheap to add. CLAP comes from clap-juce-extensions (MIT). Both are milestone 6.
- **Tests:** Catch2 for the chord engine, root tracker and note tracker, with recorded monophonic fixtures. pluginval at strictness 5. `auval` for AU.
- **Licence: AGPL-3.0.** Open source with a free licence is the case JUCE's AGPLv3 option exists for: you ship the source, and the plug-in is AGPL. Two things to check at the start:
  - The VST3 SDK's licence terms. It has been offered under GPLv3 and, in recent versions, a more permissive one. Either is compatible with AGPL, but confirm the version you pin.
  - The Sungam design system repo has no LICENSE file that I could see. If the panel art ships in a public repo, add one so the colour, type and layout rules are covered.

## UI plan

[mockup.html](mockup.html) is the reference. It is built from the real components in `_ds_bundle.js` and is interactive: drag knobs and faders, click selectors and latches, open the dropdowns, switch TRACK on. A scripted A-minor line stands in for the detected input.

**Changed: one vertical column.** The design space is 540 x 670, down from 640 x 660, and from 1220 x 820 in revision 2, scaled as one block. Sections are stacked in signal order, top to bottom. **Changed:** INPUT and HARMONY now share the top row, each narrower:

| Order | Section | Height | Contents |
|---|---|---|---|
| 1 | INPUT (168 wide) and HARMONY (320 wide), side by side | 172 | INPUT: IN and GATE, level meter, RANGE. HARMONY, row one: ROOT, TRACK, FOLLOWS, SCALE. Row two: CHORD, RANDOM, VOICES. SCALE and VOICES sit next to their neighbours instead of at the far edge |
| 2 | PROGRESSION | 158 | MODE, TENSION, HOLD on the left. On the right, three rows: DETECTED and ROOT, CHORD and TONES, a four-slot HISTORY |
| 3 | RESONATOR (376 wide) and OUTPUT (112 wide), side by side | 248 | RING, DAMP, SPREAD, GLIDE as a 2 x 2 grid, then the envelope trace over four ADSR faders. OUTPUT holds MIX, LIM and a level bar |

**How the negative space was removed.** **Changed:** the IN and OUT terminals and every trace between sections are gone, and sections sit 12 px apart instead of 20. Signal order is carried by the stacking alone, top to bottom. Two rows hold a pair of sections: INPUT with HARMONY, and RESONATOR with OUTPUT. The DETECTED readout moved from INPUT to PROGRESSION, so INPUT could narrow, and now sits with ROOT and CHORD, the other things the plug-in works out. This departs from the design system's "wired like a circuit" rule, which is deliberate, and it is the one place this panel does so.

**Changed: KEY and STYLE are one HARMONY section.** Key choices sit on the top row and chord choices on the bottom, divided by a hairline. Which controls are violet still tells you what the plug-in is changing by itself.

**Colour is signal, so the assignments are literal:**

| Colour | Used for | Why |
|---|---|---|
| Coral | INPUT, HARMONY, PROGRESSION | The primary engine: everything that decides what chord is played |
| Teal | RESONATOR, including the ADSR | The secondary engine: how the chord sounds |
| Steel | OUTPUT | Everything after the sections are summed |
| Violet | RANDOM, TRACK, FOLLOWS, and any control while the plug-in is changing it itself | Modulation, and only modulation. ROOT turns violet while tracking and CHORD while RANDOM is on |
| Amber | LIM and its lamp | The limiter, and nothing else |
| Lilac | GATE, HOLD, GLIDE | Pale trims, subordinate to their section |

**Size means importance.** MIX, RING and TENSION are the largest dials (26). MIX came down from 38 to make OUTPUT narrow, which breaks the rule that the most important control is the biggest; it keeps its position as the last control before the output. DAMP and SPREAD are 20. The trims are 15.

**What the design system does not have, and how the mockup handles it.** Two additions, both built from the system's own vocabulary:

- **Dropdown.** ROOT, SCALE and CHORD need one, because their options do not fit in a row. It is an outlined box carrying the value and a Lamp-style square that fills while the menu is open. The menu is made of vertical Selectors, so every option is labelled. SCALE's menu opens right-aligned so it stays inside the panel. A locked dropdown, as ROOT is while tracking, shows the live value and ignores clicks.
- **Envelope trace.** The system has no graph. The trace is a hairline box with one teal polyline, like the value boxes in Lacze, and a one-word label (TRANSIENT, BLOOM or PAD) that updates as the faders move.

**Rules kept from the system.** One mono face. Flat paper, no shadows, square corners. No icons or emoji. Every selector position is labelled in full. Copy is dry and uppercase.

## Milestones

Targets, not measurements.

| # | Milestone | Done when |
|---|---|---|
| 0 | JUCE scaffold, `spenningur-core` library, parameters, empty panel from `Panel.h` hue constants | Loads in a host and in pluginval |
| 1 | Pitch detector and note tracker, offline | Within 20 cents on 95% of frames in a clean mono test set at each RANGE; no spurious notes on a noise-only file |
| 2 | Chord engine and root tracker, offline | Every chord for every scale x style x mode x VOICES is in key, checked by test; RANDOM is repeatable given a seed; KEY tracking settles on the right root for a set of recorded melodies |
| 3 | Resonator bank and ADSR | Stable at RING 8 s with a full-scale input; no clicks on chord change at GLIDE 0; the internal excitation holds a pad on a plucked input |
| 4 | Wire 1 to 3 together and build the UI | The mockup, live; detected note, root and chord update from real input |
| 5 | Hardening | pluginval 5 passes; under 3% of one core at 48 kHz with 6 voices; state save and restore |
| 6 | AU and CLAP | Both load and pass `auval` and the CLAP validator |

## Risks and open questions

- **Polyphonic or noisy input breaks the detector.** One note at a time is the contract. The clarity threshold turns a bad estimate into silence rather than a wrong chord.
- **Detection latency of roughly 25 to 45 ms** delays the chord, not the dry signal. Fine for pads, noticeable for tight rhythmic parts.
- **Root tracking can be wrong for the first few notes.** KEY needs notes to settle, so it will guess early. NOTE has no such delay.
- **The 540 x 670 panel is narrow for the dropdown menus.** The SCALE menu, three columns wide, has to be right-aligned to fit. If more options are added, the menu will need a scroll.

**Questions for you:**

1. Is a portrait panel of about 540 x 670 the shape you wanted, or narrower still? Going narrower would put RESONATOR and OUTPUT on separate rows and add about 250 px of height.
2. Is TRACK with its NOTE and KEY modes right, or do you want only one?
3. What is the main source: voice, guitar, bass or synth? It changes the RANGE default.
