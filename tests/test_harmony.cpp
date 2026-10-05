#include "test.h"
#include <algorithm>
#include <cstdlib>
#include "spenningur/Harmony.h"
#include <cstdint>

using namespace spn;

namespace
{
struct Lcg
{
    uint32_t s;
    uint32_t next() { s = s * 1664525u + 1013904223u; return s >> 8; }
    int range (int lo, int hi) { return lo + (int) (next() % (uint32_t) (hi - lo + 1)); }
};

bool inScale (const ScaleDef& sc, int root, int midi)
{
    const int rel = (((midi - root) % 12) + 12) % 12;
    for (int i = 0; i < sc.count; ++i) if (sc.semis[i] == rel) return true;
    return false;
}

bool wellFormed (const Chord& c, int voices)
{
    if (! c.valid || c.count != voices) return false;
    for (int i = 0; i < c.count; ++i)
    {
        if (c.midi[i] < 36 || c.midi[i] > 96) return false;
        if (i > 0 && c.midi[i] < c.midi[i - 1]) return false;
    }
    return true;
}

HarmonyParams manual (int scale, ChordStyle st, ProgMode m, int voices, int rootPc)
{
    HarmonyParams p;
    p.rootPc = rootPc; p.trackRoot = false; p.scaleIndex = scale; p.style = st;
    p.randomStyle = false; p.voices = voices; p.mode = m; p.holdMs = 0.0f; p.tension = 0.35f;
    return p;
}

// Plays C MAJOR from C4 at the given style and returns the chord offsets from the root.
Chord cMajor (ChordStyle st, int note = 60, int voices = 6)
{
    ChordEngine e; e.prepare (1);
    HarmonyParams p = manual (0, st, ProgMode::Follow, voices, 0);
    Chord c;
    e.onNote (note, 0.0, p, c);
    return c;
}
}

TEST("harmony: every chord is in key, sorted, in range, count == voices")
{
    long chords = 0, bad = 0;
    const int roots[3] = { 0, 5, 9 };
    for (int sc = 0; sc < kNumScales; ++sc)
    for (int st = 0; st <= kNumStyles; ++st)            // kNumStyles == random style
    for (int mode = 0; mode < 3; ++mode)
    for (int v = 2; v <= kMaxVoices; ++v)
    for (int ri = 0; ri < 3; ++ri)
    for (int track = 0; track < 3; ++track)             // 0 manual, 1 Key, 2 Note
    {
        ChordEngine e; e.prepare (1234u + (uint32_t) (sc * 31 + st));
        HarmonyParams p = manual (sc, (ChordStyle) (st % kNumStyles), (ProgMode) mode, v, roots[ri]);
        p.randomStyle = st == kNumStyles;
        p.trackRoot = track != 0;
        p.follows = track == 2 ? TrackMode::Note : TrackMode::Key;
        p.tension = (float) (st % 3) * 0.5f;
        const ScaleDef& def = scaleDef (sc);
        Lcg r { 99u + (uint32_t) ri };
        double t = 0.0;
        for (int i = 0; i < 40; ++i)
        {
            t += r.range (20, 600);
            const int note = (i % 17 == 0) ? r.range (0, 127) : r.range (40, 88);
            Chord c;
            if (! e.onNote (note, t, p, c)) { ++bad; continue; }
            ++chords;
            bool ok = wellFormed (c, v);
            const int eff = e.effectiveRootPc();
            if (track == 2) ok = ok && eff == note % 12 && c.degree == 0 && c.rootPc == note % 12;
            for (int k = 0; k < c.count; ++k) ok = ok && inScale (def, eff, c.midi[k]);
            ok = ok && inScale (def, eff, c.midi[0]);
            if (! ok) ++bad;
        }
    }
    CHECK(chords > 100000);
    CHECK(bad == 0);
}

TEST("harmony: chord root sits at or below the played note within an octave")
{
    for (int note = 48; note <= 84; ++note)
    {
        ChordEngine e; e.prepare (3);
        HarmonyParams p = manual (1, ChordStyle::Triad, ProgMode::Cadence, 3, 9);
        Chord c;
        CHECK(e.onNote (note, 0.0, p, c));
        // the lowest tone of a triad is the chord root; played note is within [root, root+11] when unshifted by range
        CHECK(c.midi[0] % 12 == c.rootPc);
        CHECK(c.midi[0] <= note);
        CHECK(note - c.midi[0] < 12);
    }
}

TEST("harmony: hold suppresses close notes")
{
    ChordEngine e; e.prepare (1);
    HarmonyParams p = manual (0, ChordStyle::Triad, ProgMode::Follow, 3, 0);
    p.holdMs = 300.0f;
    Chord c, c2;
    CHECK(e.onNote (60, 1000.0, p, c));
    CHECK(! e.onNote (67, 1100.0, p, c2));
    CHECK(! e.onNote (64, 1299.0, p, c2));
    CHECK(e.current().midi[0] == c.midi[0] && e.current().rootPc == c.rootPc);
    CHECK(e.onNote (67, 1300.0, p, c2));
    CHECK(! e.onNote (60, 1400.0, p, c2));       // hold restarts from the accepted change
    CHECK(e.onNote (60, 1600.0, p, c2));
}

TEST("harmony: determinism for a seed, difference between seeds")
{
    auto run = [] (uint32_t seed, ProgMode mode, int* out, int cap)
    {
        ChordEngine e; e.prepare (seed);
        HarmonyParams p = manual (0, ChordStyle::Triad, mode, 5, 0);
        p.randomStyle = true; p.holdMs = 0.0f;
        Lcg r { 7u };
        int k = 0;
        for (int i = 0; i < 200; ++i)
        {
            Chord c;
            e.onNote (r.range (48, 84), i * 100.0, p, c);
            for (int j = 0; j < c.count && k < cap; ++j) out[k++] = c.midi[j];
            if (k < cap) out[k++] = (int) c.style * 100 + c.degree;
        }
        return k;
    };
    static int a[4000], b[4000], c[4000];
    for (int m = 0; m < 3; ++m)
    {
        const int na = run (42, (ProgMode) m, a, 4000), nb = run (42, (ProgMode) m, b, 4000), nc = run (43, (ProgMode) m, c, 4000);
        CHECK(na == nb); CHECK(na == nc);
        bool same = true, diff = false;
        for (int i = 0; i < na; ++i) { same = same && a[i] == b[i]; diff = diff || a[i] != c[i]; }
        CHECK(same);
        CHECK(diff);
    }
    // seed 0 is accepted and deterministic too
    CHECK(run (0, ProgMode::Wander, a, 4000) == run (0, ProgMode::Wander, b, 4000));
}

TEST("harmony: random style draws all styles and respects voices")
{
    ChordEngine e; e.prepare (5);
    HarmonyParams p = manual (0, ChordStyle::Triad, ProgMode::Follow, 4, 0);
    p.randomStyle = true;
    bool seen[kNumStyles] = {};
    for (int i = 0; i < 400; ++i)
    {
        Chord c; e.onNote (60, i * 10.0, p, c);
        seen[(int) c.style] = true;
        CHECK(c.count == 4);
    }
    for (int s = 0; s < kNumStyles; ++s) CHECK(seen[s]);
}

TEST("harmony: RootTracker settles on A for an A-minor melody and ignores one stray note")
{
    RootTracker t; t.reset();
    const ScaleDef& minor = scaleDef (1);
    const int mel[] = { 9, 0, 4, 9, 11, 2, 4, 9, 7, 5, 4, 2, 0, 9 };    // A C E A B D E A G F E D C A
    double now = 0.0;
    int root = 0;
    for (int rep = 0; rep < 4; ++rep)
        for (int pc : mel) { t.noteOn (pc, now); now += 400.0; root = t.keyRoot (minor, root, now); }
    CHECK(root == 9);
    t.noteOn (6, now);                           // F#, out of A minor
    now += 200.0;
    CHECK(t.keyRoot (minor, root, now) == 9);
    // with nothing known, the best candidate is taken (currentRoot invalid)
    RootTracker u; u.reset();
    u.noteOn (2, 0.0);
    CHECK(u.keyRoot (minor, -1, 1.0) == 2);
}

TEST("harmony: RootTracker forgets with a ~12 s half-life")
{
    RootTracker t; t.reset();
    const ScaleDef& major = scaleDef (0);
    for (int i = 0; i < 8; ++i) t.noteOn (0, i * 100.0);       // C-ish
    int root = t.keyRoot (major, -1, 800.0);
    CHECK(root == 0);
    // a minute later, G notes dominate despite older C weight
    for (int i = 0; i < 4; ++i) t.noteOn (7, 60000.0 + i * 100.0);
    root = t.keyRoot (major, root, 60400.0);
    CHECK(root == 7);
}

TEST("harmony: Key tracking through the engine settles on A and keeps it")
{
    ChordEngine e; e.prepare (1);
    HarmonyParams p = manual (1, ChordStyle::Seventh, ProgMode::Follow, 4, 3);
    Chord c;
    // start with a hand root elsewhere, then hand over to tracking
    e.onNote (60, 0.0, p, c);
    CHECK(e.effectiveRootPc() == 3);
    p.trackRoot = true; p.follows = TrackMode::Key;
    const int mel[] = { 57, 60, 64, 69, 71, 62, 64, 69, 67, 65, 64, 62, 60, 57 };
    double now = 10.0;
    for (int rep = 0; rep < 4; ++rep)
        for (int n : mel) { e.onNote (n, now, p, c); now += 350.0; }
    CHECK(e.effectiveRootPc() == 9);
    e.onNote (66, now, p, c);                    // F#: out of key, snaps
    CHECK(e.effectiveRootPc() == 9);
    CHECK(c.valid);
}

TEST("harmony: Note tracking gives root = played pitch class and degree 0")
{
    for (int sc = 0; sc < kNumScales; ++sc)
        for (int note = 40; note < 90; ++note)
        {
            ChordEngine e; e.prepare (1);
            HarmonyParams p = manual (sc, ChordStyle::Triad, ProgMode::Wander, 3, 0);
            p.trackRoot = true; p.follows = TrackMode::Note;
            Chord c;
            CHECK(e.onNote (note, 0.0, p, c));
            CHECK(c.degree == 0);
            CHECK(c.rootPc == note % 12);
            CHECK(e.effectiveRootPc() == note % 12);
            CHECK(c.midi[0] % 12 == note % 12);
        }
}

TEST("harmony: Cadence prefers the documented root motion")
{
    HarmonyParams p = manual (0, ChordStyle::Triad, ProgMode::Cadence, 3, 0);
    p.tension = 0.0f;
    auto second = [&] (int first, int next)
    {
        ChordEngine e; e.prepare (1);
        Chord c; e.onNote (first, 0.0, p, c); e.onNote (next, 1000.0, p, c);
        return c.degree;
    };
    CHECK(second (60, 67) == 4);    // G: stay on the played degree, +4 (fifth up)
    CHECK(second (60, 65) == 3);    // F: +3
    CHECK(second (60, 62) == 4);    // D: candidates 1,6,4 -> +4 (V) beats +1
    CHECK(second (60, 64) == 5);    // E: candidates 2,0,5 -> +5 (vi)
    // from the V chord (degree 4), played C (d=0): candidates 0,5,3 -> +3 = degree 0? motion 0-4=3 mod 7
    CHECK(second (67, 60) == 0);
    // first chord has no history: the played degree
    ChordEngine e; e.prepare (1); Chord c; e.onNote (64, 0.0, p, c);
    CHECK(c.degree == 2);
}

TEST("harmony: Follow minimises degree distance")
{
    HarmonyParams p = manual (0, ChordStyle::Triad, ProgMode::Follow, 3, 0);
    p.tension = 0.0f;
    ChordEngine e; e.prepare (1);
    Chord c; e.onNote (60, 0.0, p, c);          // C -> degree 0
    CHECK(c.degree == 0);
    e.onNote (69, 1000.0, p, c);                // A (d=5): candidates 5,3,1 -> 1 is closest to 0 (distance 1)
    CHECK(c.degree == 1);
    e.onNote (72, 2000.0, p, c);                // C (d=0): candidates 0,5,3 -> 0 is at distance 1 from 1
    CHECK(c.degree == 0);
    e.onNote (71, 3000.0, p, c);                // B (d=6): candidates 6,4,2 -> 6 (distance 1 circular)
    CHECK(c.degree == 6);
    // and across all scales Follow never picks a candidate that is further than another
    for (int sc = 0; sc < kNumScales; ++sc)
    {
        const int n = scaleDef (sc).count;
        ChordEngine f; f.prepare (2);
        HarmonyParams q = manual (sc, ChordStyle::Triad, ProgMode::Follow, 3, 0);
        q.tension = 0.0f;
        Lcg r { 5u };
        int prev = -1;
        for (int i = 0; i < 100; ++i)
        {
            Chord cc;
            const ScaleDef& def = scaleDef (sc);
            const int d = r.range (0, n - 1);
            const int note = 60 + def.semis[d];      // in scale, so the played degree is d
            f.onNote (note, i * 100.0, q, cc);
            if (prev >= 0)
            {
                int bestDist = 99;
                const int cand[3] = { d, ((d - 2) % n + n) % n, ((d - 4) % n + n) % n };
                for (int k = 0; k < 3; ++k)
                {
                    const int dd = ((cand[k] - prev) % n + n) % n;
                    bestDist = std::min (bestDist, std::min (dd, n - dd));
                }
                const int dd = ((cc.degree - prev) % n + n) % n;
                CHECK(std::min (dd, n - dd) == bestDist);
            }
            prev = cc.degree;
        }
    }
}

TEST("harmony: out-of-key played notes snap to the nearest scale degree")
{
    // C MAJOR, played C# (between C and D, tie -> lower = C, degree 0) with a Power chord
    ChordEngine e; e.prepare (1);
    HarmonyParams p = manual (0, ChordStyle::Power, ProgMode::Cadence, 2, 0);
    Chord c; e.onNote (61, 0.0, p, c);
    CHECK(c.degree == 0);
    // played F# in C major: tie between F and G -> F (degree 3)
    ChordEngine e2; e2.prepare (1);
    e2.onNote (66, 0.0, p, c);
    CHECK(c.degree == 3);
    // Bb in C major snaps to B (degree 6) or A: tie 9/11 vs 10 -> A (degree 5)
    ChordEngine e3; e3.prepare (1);
    e3.onNote (70, 0.0, p, c);
    CHECK(c.degree == 5);
}

TEST("harmony: style step tables give the right intervals (C MAJOR from C4)")
{
    auto expect = [] (ChordStyle st, int voices, std::initializer_list<int> notes)
    {
        Chord c = cMajor (st, 60, voices);
        CHECK(c.count == (int) notes.size());
        int i = 0;
        for (int n : notes) { CHECK(i < c.count && c.midi[i] == n); ++i; }
    };
    expect (ChordStyle::Triad,   3, { 60, 64, 67 });
    expect (ChordStyle::Seventh, 4, { 60, 64, 67, 71 });
    expect (ChordStyle::Ninth,   5, { 60, 64, 67, 71, 74 });
    expect (ChordStyle::Sus2,    3, { 60, 62, 67 });
    expect (ChordStyle::Sus4,    3, { 60, 65, 67 });
    expect (ChordStyle::Add9,    4, { 60, 64, 67, 74 });
    expect (ChordStyle::Power,   2, { 60, 67 });
    expect (ChordStyle::Quartal, 3, { 60, 65, 71 });
    expect (ChordStyle::Shell,   3, { 60, 64, 71 });
    expect (ChordStyle::Drone,   3, { 60, 67, 72 });
}

TEST("harmony: VOICES cuts from the top and doubles an octave up")
{
    // cut: 7TH at 2 and 3 voices keeps root, third, fifth first
    Chord c = cMajor (ChordStyle::Seventh, 60, 2);
    CHECK(c.count == 2 && c.midi[0] == 60 && c.midi[1] == 64);
    c = cMajor (ChordStyle::Ninth, 60, 3);
    CHECK(c.count == 3 && c.midi[2] == 67);
    // fill: triad at 5 voices -> + root, third an octave up
    c = cMajor (ChordStyle::Triad, 60, 5);
    CHECK(c.count == 5);
    const int e5[5] = { 60, 64, 67, 72, 76 };
    for (int i = 0; i < 5; ++i) CHECK(c.midi[i] == e5[i]);
    // power at 6: 0 4 | +oct: 0 4 | +2 oct: 0 4
    c = cMajor (ChordStyle::Power, 60, 6);
    const int p6[6] = { 60, 67, 72, 79, 84, 91 };
    for (int i = 0; i < 6; ++i) CHECK(c.midi[i] == p6[i]);
    // drone at 4: no duplicated root octave
    c = cMajor (ChordStyle::Drone, 60, 4);
    for (int i = 1; i < c.count; ++i) CHECK(c.midi[i] > c.midi[i - 1]);
}

TEST("harmony: register moves whole octaves into MIDI 36..96")
{
    Chord c = cMajor (ChordStyle::Ninth, 24, 5);
    CHECK(c.midi[0] >= 36 && c.midi[0] % 12 == 0);
    c = cMajor (ChordStyle::Ninth, 127, 5);
    CHECK(c.midi[c.count - 1] <= 96);
    c = cMajor (ChordStyle::Power, 120, 6);
    CHECK(c.midi[5] <= 96 && c.midi[0] >= 36);
}

TEST("harmony: tension is deterministic and moves away from the tonic")
{
    // Follow from degree 1, played C (d=0): candidates 0,5,3. Distances from 1: 1,3,2 -> tonic at zero tension.
    auto run = [] (float tension)
    {
        ChordEngine e; e.prepare (1);
        HarmonyParams p = manual (0, ChordStyle::Triad, ProgMode::Follow, 3, 0);
        p.tension = tension;
        Chord c; e.onNote (62, 0.0, p, c);      // D, d=1
        e.onNote (72, 1000.0, p, c);
        return c.degree;
    };
    CHECK(run (0.0f) == 0);
    CHECK(run (0.35f) == 0);
    CHECK(run (1.0f) != 0);
    CHECK(run (1.0f) == run (1.0f));
}

TEST("harmony: wander covers its pool and tension widens it")
{
    auto degrees = [] (float tension)
    {
        ChordEngine e; e.prepare (11);
        HarmonyParams p = manual (0, ChordStyle::Triad, ProgMode::Wander, 3, 0);
        p.tension = tension;
        bool seen[7] = {};
        for (int i = 0; i < 300; ++i) { Chord c; e.onNote (67, i * 10.0, p, c); seen[c.degree] = true; }
        int k = 0; for (bool b : seen) k += b;
        return k;
    };
    CHECK(degrees (0.2f) == 2);     // G: degrees 4 and 2
    CHECK(degrees (0.8f) == 3);     // plus degree 0
}
