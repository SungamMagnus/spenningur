# Spenningur

A resonating chord harmoniser. It listens to a single melodic line, works out which note is being played, picks a chord for it from the scale and style you set, and rings that chord around the note through a bank of tuned resonators. Your playing is the excitation, so the attack and tone of the ring are yours.

The root can be set by hand or follow the input. Chords follow a progression, not a fixed harmony per pitch. The style can be drawn at random for every chord. An ADSR decides whether the result is a transient pluck or a pad.

The design is in [docs/PLAN.md](docs/PLAN.md). The panel is specified by [docs/mockup/index.html](docs/mockup/index.html), which is interactive.

## Layout

| Path | What |
|---|---|
| `core/` | Detector, harmony engine, resonator. No JUCE, no allocation on the audio path |
| `tests/` | Unit tests for the core, with a tiny built-in harness |
| `Source/` | The JUCE plug-in: parameters, processor, editor, panel drawing |
| `tools/` | `panel_shot` renders the editor to a PNG, `dsp_check` runs the real processor over synthetic melodies |
| `docs/` | The plan and the mockup |

## Build

JUCE is expected at `/Applications/JUCE` (`-DJUCE_DIR=...` to change).

```bash
# core and tests only, no JUCE
cmake -S . -B build/core -DSPENNINGUR_BUILD_PLUGIN=OFF
cmake --build build/core -j8 && ./build/core/spn_tests

# the plug-in (VST3, AU, Standalone) and the dev tools
cmake -S . -B build/plugin -DSPENNINGUR_DEV_TOOLS=ON
cmake --build build/plugin -j8
```

Pass `-DCMAKE_OSX_ARCHITECTURES=arm64` for a faster development build. The default is a universal binary.

## Licence

AGPL-3.0, because it links JUCE under its AGPL option. See `LICENSE`.
