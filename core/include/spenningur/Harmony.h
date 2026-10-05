#pragma once

#include <cstdint>

/* Scales, chord styles, root tracking and the chord engine. Pure logic: no
 * JUCE, no allocation, no audio. Public API is a contract shared with the
 * plug-in; the private sections belong to the implementation. */
namespace spn
{

constexpr int kMaxVoices = 6;
constexpr int kNumScales = 11;
constexpr int kNumStyles = 10;

/** Scales in panel order: MAJOR, MINOR, DORIAN, PHRYGIAN, LYDIAN, MIXOLYDIAN,
 *  HARM MINOR, MEL MINOR, PENT MAJOR, PENT MINOR, BLUES. */
struct ScaleDef
{
    const char* name;
    int count;          // 5, 6 or 7
    int semis[7];       // ascending, from 0
};
const ScaleDef& scaleDef (int index);          // clamped to 0..kNumScales-1

/** Chord styles in panel order. Each is a stack of scale steps from the chord
 *  root, so any chord is in key by construction:
 *    Triad 0 2 4 | Seventh 0 2 4 6 | Ninth 0 2 4 6 8 | Sus2 0 1 4 | Sus4 0 3 4
 *    Add9 0 2 4 8 | Power 0 4 | Quartal 0 3 6 | Shell 0 2 6 | Drone 0 4 octave
 *  ("octave" is a step of scale.count, i.e. the root again one octave up). */
enum class ChordStyle : int { Triad, Seventh, Ninth, Sus2, Sus4, Add9, Power, Quartal, Shell, Drone };
const char* styleName (ChordStyle);            // "TRIAD", "7TH", "9TH", "SUS2", "SUS4", "ADD9", "POWER", "QUARTAL", "SHELL", "DRONE"
const char* rootName (int pitchClass);         // "C", "C#", ... "B"

enum class ProgMode  : int { Follow, Cadence, Wander };
enum class TrackMode : int { Note, Key };

struct HarmonyParams
{
    int        rootPc      = 9;                 // 0..11, used when trackRoot is false
    bool       trackRoot   = false;
    TrackMode  follows     = TrackMode::Key;
    int        scaleIndex  = 1;
    ChordStyle style       = ChordStyle::Seventh;
    bool       randomStyle = true;              // draw a style per chord change
    int        voices      = 5;                 // 2..kMaxVoices
    ProgMode   mode        = ProgMode::Cadence;
    float      tension     = 0.35f;             // 0..1
    float      holdMs      = 300.0f;            // chord changes closer together than this are ignored
};

/** A chord as absolute MIDI notes, lowest first. */
struct Chord
{
    bool       valid  = false;
    int        rootPc = 0;                      // chord root pitch class
    int        degree = 0;                      // scale degree of the chord root, 0-based
    ChordStyle style  = ChordStyle::Triad;
    int        count  = 0;                      // 2..kMaxVoices
    int        midi[kMaxVoices] = {};
};

/** Estimates the tonal centre from the notes played.
 *
 *  Keeps a pitch-class histogram with a fixed ~12 s half-life (time-based, from
 *  the nowMs passed in). keyRoot() scores all 12 roots against the scale (notes
 *  in the scale count 1, the candidate root itself counts 3) and only replaces
 *  `currentRoot` when the best candidate beats it by 20% — a passing out-of-key
 *  note must not flip the key. */
class RootTracker
{
public:
    void reset();
    void noteOn (int pitchClass, double nowMs);
    int  keyRoot (const ScaleDef& scale, int currentRoot, double nowMs);

private:
    float hist[12] = {};
    double lastMs = 0.0;
};

/** Picks chords for a stream of detected notes. */
class ChordEngine
{
public:
    void prepare (uint32_t seed);               // seeds the xorshift RNG; deterministic given a seed
    void reset();

    /** A stable note arrived. Returns true and fills `out` when the chord
     *  changed. Notes arriving within params.holdMs of the last chord change
     *  update internal state (root tracker, detected note) but return false.
     *  nowMs must be monotonic. Real-time safe. */
    bool onNote (int midiNote, double nowMs, const HarmonyParams& params, Chord& out);

    const Chord& current() const { return chord; }
    int effectiveRootPc() const { return root; }      // the root in use (hand-set or tracked)
    ChordStyle currentStyle() const { return chord.style; }

private:
    Chord chord;
    int root = 9, prevDegree = -1;
    double lastChangeMs = -1.0e9;
    uint32_t rng = 0x9e3779b9u;
    RootTracker tracker;
};

} // namespace spn

/* Voicing notes for the implementation:
 *  - The detected note sits at scale degree d (nearest scale tone to the played
 *    note, relative to the effective root). In a 7-note scale it belongs to
 *    three diatonic triads rooted at d, d-2 (it is the third) and d-4 (the fifth).
 *    These are the candidate chord roots. For 5- and 6-note scales use the same
 *    d, d-2, d-4 on scale steps (mod count).
 *  - Follow: candidate with the smallest circular scale-step distance to the
 *    previous chord's degree. Cadence: prefer a move of +3, +4, +1 or +5 steps
 *    (mod count) from the previous degree, else d. Wander: random among the
 *    candidates; tension > 0.5 widens the pool from 2 to all 3.
 *  - Tension also biases toward extensions and away from the tonic; keep it
 *    subtle and deterministic given the seed.
 *  - TrackMode::Note with trackRoot: root = the played note's pitch class and
 *    the chord root is degree 0 (MODE is bypassed). TrackMode::Key: root from
 *    RootTracker::keyRoot, MODE applies normally.
 *  - Style: randomStyle draws one of the 10 styles per chord change from the
 *    RNG; otherwise params.style.
 *  - Voices: stack the style's steps; if the style has more tones than
 *    params.voices cut from the top; if fewer, double tones an octave up
 *    (step + count) cycling from the start.
 *  - Register: place the chord root at or below the played note, within an
 *    octave, then keep every tone inside MIDI 36..96 by moving whole octaves.
 */
