// Scale/name tables, RootTracker and ChordEngine. See Harmony.h for the contract.
#include "spenningur/Harmony.h"

#include <cmath>

namespace spn
{
static const ScaleDef kScaleTable[kNumScales] = {
    { "MAJOR",       7, { 0, 2, 4, 5, 7, 9, 11 } },
    { "MINOR",       7, { 0, 2, 3, 5, 7, 8, 10 } },
    { "DORIAN",      7, { 0, 2, 3, 5, 7, 9, 10 } },
    { "PHRYGIAN",    7, { 0, 1, 3, 5, 7, 8, 10 } },
    { "LYDIAN",      7, { 0, 2, 4, 6, 7, 9, 11 } },
    { "MIXOLYDIAN",  7, { 0, 2, 4, 5, 7, 9, 10 } },
    { "HARM MINOR",  7, { 0, 2, 3, 5, 7, 8, 11 } },
    { "MEL MINOR",   7, { 0, 2, 3, 5, 7, 9, 11 } },
    { "PENT MAJOR",  5, { 0, 2, 4, 7, 9 } },
    { "PENT MINOR",  5, { 0, 3, 5, 7, 10 } },
    { "BLUES",       6, { 0, 3, 5, 6, 7, 10 } },
};

const ScaleDef& scaleDef (int index)
{
    return kScaleTable[index < 0 ? 0 : index >= kNumScales ? kNumScales - 1 : index];
}

const char* styleName (ChordStyle s)
{
    static const char* names[kNumStyles] = { "TRIAD", "7TH", "9TH", "SUS2", "SUS4", "ADD9", "POWER", "QUARTAL", "SHELL", "DRONE" };
    const int i = (int) s;
    return names[i < 0 ? 0 : i >= kNumStyles ? kNumStyles - 1 : i];
}

const char* rootName (int pc)
{
    static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return names[((pc % 12) + 12) % 12];
}

namespace
{
constexpr double kHalfLifeMs  = 12000.0;
constexpr float  kKeyMargin   = 1.2f;     // best root must beat the current by 20%
constexpr float  kRootWeight  = 3.0f;
constexpr int    kLowestMidi  = 36;
constexpr int    kHighestMidi = 96;

inline int mod12 (int x) { return ((x % 12) + 12) % 12; }
inline int modN (int x, int n) { return ((x % n) + n) % n; }

struct StyleDef { int count; int steps[5]; };     // steps: -1 never used; kOctave marks "scale.count"
constexpr int kOctave = -2;
const StyleDef kStyleTable[kNumStyles] = {
    { 3, { 0, 2, 4 } },            // Triad
    { 4, { 0, 2, 4, 6 } },         // Seventh
    { 5, { 0, 2, 4, 6, 8 } },      // Ninth
    { 3, { 0, 1, 4 } },            // Sus2
    { 3, { 0, 3, 4 } },            // Sus4
    { 4, { 0, 2, 4, 8 } },         // Add9
    { 2, { 0, 4 } },               // Power
    { 3, { 0, 3, 6 } },            // Quartal
    { 3, { 0, 2, 6 } },            // Shell
    { 3, { 0, 4, kOctave } },      // Drone
};

/** Nearest scale degree to a pitch class measured from the scale root. Ties go to the lower tone. */
int snapDegree (const ScaleDef& sc, int pcRel)
{
    int best = 0, bestDist = 99, bestPos = 99;
    for (int i = 0; i < sc.count; ++i)
        for (int o = -1; o <= 1; ++o)
        {
            const int pos = sc.semis[i] + 12 * o;
            const int dist = pos > pcRel ? pos - pcRel : pcRel - pos;
            if (dist < bestDist || (dist == bestDist && pos < bestPos))
            {
                bestDist = dist; bestPos = pos; best = i;
            }
        }
    return best;
}

inline int circDist (int a, int b, int n)
{
    const int d = modN (a - b, n);
    return d < n - d ? d : n - d;
}

inline uint32_t nextRng (uint32_t& s)
{
    s ^= s << 13; s ^= s >> 17; s ^= s << 5;
    return s;
}

inline float rand01 (uint32_t& s) { return (float) (nextRng (s) >> 8) * (1.0f / 16777216.0f); }
} // namespace

void RootTracker::reset() { for (float& h : hist) h = 0.0f; lastMs = 0.0; }

void RootTracker::noteOn (int pitchClass, double nowMs)
{
    // decay lazily from the time of the last update
    if (nowMs > lastMs)
    {
        const float k = (float) std::exp2 (-(nowMs - lastMs) / kHalfLifeMs);
        for (float& h : hist) h *= k;
        lastMs = nowMs;
    }
    hist[mod12 (pitchClass)] += 1.0f;
}

int RootTracker::keyRoot (const ScaleDef& scale, int currentRoot, double nowMs)
{
    if (nowMs > lastMs)
    {
        const float k = (float) std::exp2 (-(nowMs - lastMs) / kHalfLifeMs);
        for (float& h : hist) h *= k;
        lastMs = nowMs;
    }

    bool inScale[12] = {};
    for (int i = 0; i < scale.count; ++i) inScale[scale.semis[i] % 12] = true;

    float score[12];
    for (int r = 0; r < 12; ++r)
    {
        float s = 0.0f;
        for (int pc = 0; pc < 12; ++pc)
        {
            const int rel = mod12 (pc - r);
            if (rel == 0)        s += kRootWeight * hist[pc];
            else if (inScale[rel]) s += hist[pc];
        }
        score[r] = s;
    }

    int best = 0;
    for (int r = 1; r < 12; ++r) if (score[r] > score[best]) best = r;

    if (currentRoot < 0 || currentRoot > 11) return best;
    return score[best] > score[currentRoot] * kKeyMargin ? best : currentRoot;
}

void ChordEngine::prepare (uint32_t seed)
{
    rng = seed ? seed : 0x9e3779b9u;
    reset();
}

void ChordEngine::reset()
{
    chord = Chord{};
    root = 9;
    prevDegree = -1;
    lastChangeMs = -1.0e9;
    tracker.reset();
}

bool ChordEngine::onNote (int midiNote, double nowMs, const HarmonyParams& p, Chord& out)
{
    midiNote = midiNote < 0 ? 0 : midiNote > 127 ? 127 : midiNote;
    const int notePc = midiNote % 12;
    const ScaleDef& sc = scaleDef (p.scaleIndex);
    const int n = sc.count;

    tracker.noteOn (notePc, nowMs);

    const bool held = (nowMs - lastChangeMs) < (double) p.holdMs;
    const bool noteMode = p.trackRoot && p.follows == TrackMode::Note;

    // effective root: hand-set, tracked tonal centre, or (below) the played note
    if (! p.trackRoot)
        root = mod12 (p.rootPc);
    else if (p.follows == TrackMode::Key)
        root = tracker.keyRoot (sc, chord.valid ? root : -1, nowMs);

    if (held)
        return false;

    int degree = 0;
    if (noteMode)
    {
        root = notePc;                       // chord root is degree 0 by definition; MODE bypassed
    }
    else
    {
        const int d = snapDegree (sc, mod12 (notePc - root));
        const int cand[3] = { d, modN (d - 2, n), modN (d - 4, n) };

        // reference: the previous chord root, expressed in the (possibly new) root's scale
        const bool havePrev = chord.valid;
        const int ref = havePrev ? snapDegree (sc, mod12 (chord.rootPc - root)) : 0;
        const float tension = p.tension < 0.0f ? 0.0f : p.tension > 1.0f ? 1.0f : p.tension;
        const float tonicPenalty = tension * 2.5f;     // pushes away from the tonic, never overrides a clear mode preference at low tension

        if (p.mode != ProgMode::Wander && ! havePrev)
        {
            degree = d;                                // nothing to follow yet: the played degree
        }
        else if (p.mode == ProgMode::Wander)
        {
            const int pool = tension > 0.5f ? 3 : 2;
            float w[3], total = 0.0f;
            for (int i = 0; i < pool; ++i)
            {
                w[i] = cand[i] == 0 ? 1.0f - 0.5f * tension : 1.0f;
                total += w[i];
            }
            float u = rand01 (rng) * total;
            int pick = pool - 1;
            for (int i = 0; i < pool; ++i)
            {
                if (u < w[i]) { pick = i; break; }
                u -= w[i];
            }
            degree = cand[pick];
        }
        else
        {
            static const int kCadencePrefs[4] = { 3, 4, 1, 5 };
            float bestScore = -1.0e9f;
            for (int i = 0; i < 3; ++i)
            {
                float s = 0.0f;
                if (havePrev)
                {
                    if (p.mode == ProgMode::Follow)
                    {
                        s = -(float) circDist (cand[i], ref, n);
                    }
                    else
                    {
                        const int motion = modN (cand[i] - ref, n);
                        for (int k = 0; k < 4; ++k)
                            if (modN (kCadencePrefs[k], n) != 0 && motion == modN (kCadencePrefs[k], n))
                            {
                                s = (float) (4 - k);
                                break;
                            }
                    }
                }
                if (i == 0) s += 0.5f;                     // tie-break and cadence fallback: the played degree itself
                if (cand[i] == 0) s -= tonicPenalty;
                if (s > bestScore) { bestScore = s; degree = cand[i]; }
            }
        }
    }

    // style
    ChordStyle style = p.style;
    if (p.randomStyle)
    {
        const float tension = p.tension < 0.0f ? 0.0f : p.tension > 1.0f ? 1.0f : p.tension;
        float w[kNumStyles], total = 0.0f;
        for (int i = 0; i < kNumStyles; ++i)
        {
            switch ((ChordStyle) i)
            {
                case ChordStyle::Seventh: case ChordStyle::Ninth: case ChordStyle::Add9:
                case ChordStyle::Quartal: case ChordStyle::Shell:
                    w[i] = 1.0f + tension; break;
                case ChordStyle::Triad: case ChordStyle::Power: case ChordStyle::Drone:
                    w[i] = 1.0f - 0.6f * tension; break;
                default: w[i] = 1.0f; break;
            }
            total += w[i];
        }
        float u = rand01 (rng) * total;
        int pick = kNumStyles - 1;
        for (int i = 0; i < kNumStyles; ++i)
        {
            if (u < w[i]) { pick = i; break; }
            u -= w[i];
        }
        style = (ChordStyle) pick;
    }
    {
        const int si = (int) style;
        style = (ChordStyle) (si < 0 ? 0 : si >= kNumStyles ? kNumStyles - 1 : si);
    }

    // stack the steps, then fit to VOICES
    const StyleDef& sd = kStyleTable[(int) style];
    int base[5];
    for (int i = 0; i < sd.count; ++i) base[i] = sd.steps[i] == kOctave ? n : sd.steps[i];

    const int voices = p.voices < 2 ? 2 : p.voices > kMaxVoices ? kMaxVoices : p.voices;
    int steps[kMaxVoices];
    int count = 0;
    for (int i = 0; i < sd.count && count < voices; ++i) steps[count++] = base[i];
    for (int k = 0, oct = 1; count < voices; )
    {
        const int s = base[k] + n * oct;
        bool dup = false;
        for (int i = 0; i < count; ++i) dup = dup || steps[i] == s;
        if (! dup) steps[count++] = s;
        if (++k == sd.count) { k = 0; ++oct; }
    }

    // register: chord root at or below the played note, within an octave
    const int chordPc = mod12 (root + sc.semis[degree]);
    const int rootMidi = midiNote - mod12 (midiNote - chordPc);

    int tones[kMaxVoices];
    for (int i = 0; i < count; ++i)
    {
        const int k = degree + steps[i];
        tones[i] = rootMidi - sc.semis[degree] + sc.semis[k % n] + 12 * (k / n);
    }
    // sort ascending (tiny insertion sort)
    for (int i = 1; i < count; ++i)
        for (int j = i; j > 0 && tones[j - 1] > tones[j]; --j)
        { const int t = tones[j]; tones[j] = tones[j - 1]; tones[j - 1] = t; }

    int shift = 0;
    while (tones[count - 1] + shift > kHighestMidi) shift -= 12;
    while (tones[0] + shift < kLowestMidi) shift += 12;

    chord.valid = true;
    chord.rootPc = chordPc;
    chord.degree = degree;
    chord.style = style;
    chord.count = count;
    for (int i = 0; i < kMaxVoices; ++i) chord.midi[i] = i < count ? tones[i] + shift : 0;

    prevDegree = degree;
    lastChangeMs = nowMs;
    out = chord;
    return true;
}
} // namespace spn
