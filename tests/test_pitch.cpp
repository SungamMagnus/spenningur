// Pitch detector and note tracker tests. Everything is synthetic, deterministic audio.
#include "test.h"
#include "spenningur/Pitch.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <vector>

using namespace spn;

namespace
{
constexpr double kPi = 3.14159265358979323846;

enum class Timbre { Sine, Saw, WeakFundamental };

// Band-limited tones by additive synthesis, so there is no aliasing in the test material.
void addTone (std::vector<float>& buf, size_t from, size_t to, double sr, double hz, double amp,
              Timbre t, double& phase)
{
    const int maxH = (int) std::min (40.0, std::floor (0.45 * sr / hz));
    for (size_t i = from; i < to; ++i)
    {
        double s = 0.0;
        if (t == Timbre::Sine)
            s = std::sin (phase);
        else if (t == Timbre::Saw)
        {
            for (int h = 1; h <= maxH; ++h) s += std::sin (h * phase) / h;
            s *= 0.6;
        }
        else   // fundamental at -26 dB relative to a strong 2nd..5th harmonic cluster
        {
            s = 0.05 * std::sin (phase);
            for (int h = 2; h <= std::min (5, maxH); ++h) s += (h == 3 ? 0.5 : 0.3) * std::sin (h * phase);
        }
        buf[i] += (float) (amp * s);
        phase += 2.0 * kPi * hz / sr;
        if (phase > 2.0 * kPi) phase -= 2.0 * kPi;
    }
}

std::vector<float> tone (double sr, double seconds, double hz, double amp, Timbre t)
{
    std::vector<float> b ((size_t) (sr * seconds), 0.0f);
    double ph = 0.0;
    addTone (b, 0, b.size(), sr, hz, amp, t, ph);
    return b;
}

struct Run
{
    std::vector<Estimate> est;
};

Run runDetector (PitchDetector& d, const std::vector<float>& x, int block = 480)
{
    Run r;
    Estimate tmp[16];
    for (size_t pos = 0; pos < x.size(); pos += (size_t) block)
    {
        const int n = (int) std::min ((size_t) block, x.size() - pos);
        const int got = d.process (x.data() + pos, n, tmp, 16);
        for (int i = 0; i < got; ++i) r.est.push_back (tmp[i]);
    }
    return r;
}

double centsDiff (double hz, double ref) { return 1200.0 * std::log2 (hz / ref); }

struct Stats { int frames = 0, good = 0; double worst = 0.0; };

Stats scoreTone (Range range, double sr, double hz, Timbre t, double amp = 0.5)
{
    PitchDetector d;
    d.prepare (sr, range);
    const double secs = (d.windowSamples() * 2.5 + 8192) / sr;
    auto x = tone (sr, secs, hz, amp, t);
    auto r = runDetector (d, x);
    Stats s;
    const size_t skip = (size_t) (d.windowSamples() / d.hopSamples()) + 2;
    for (size_t i = skip; i < r.est.size(); ++i)
    {
        ++s.frames;
        const auto& e = r.est[i];
        if (! e.valid) { s.worst = 1e9; continue; }
        const double c = std::fabs (centsDiff (e.hz, hz));
        s.worst = std::max (s.worst, c);
        if (c <= 20.0) ++s.good;
    }
    return s;
}

std::vector<double> pitchesFor (Range r)
{
    switch (r)
    {
        case Range::Small:  return { 131.0, 164.8, 196.0, 261.6, 330.0, 440.0, 500.0 };
        case Range::Medium: return { 85.0, 110.0, 196.0, 329.6, 659.3, 1000.0, 1300.0 };
        default:            return { 32.0, 41.2, 55.0, 110.0, 440.0, 1000.0, 1760.0, 2050.0 };
    }
}

void accuracyForRange (Range range, const char* name)
{
    int frames = 0, good = 0;
    for (double sr : { 44100.0, 48000.0 })
        for (Timbre t : { Timbre::Sine, Timbre::Saw, Timbre::WeakFundamental })
            for (double hz : pitchesFor (range))
            {
                const auto s = scoreTone (range, sr, hz, t);
                frames += s.frames; good += s.good;
                // Every single case should be solid, not only the aggregate.
                const bool okCase = s.frames > 0 && s.good >= s.frames - 1;
                if (! okCase)
                    std::printf ("    case %s sr=%.0f hz=%.1f timbre=%d: %d/%d within 20c, worst %.1f c\n",
                                 name, sr, hz, (int) t, s.good, s.frames, s.worst);
                CHECK (okCase);
            }
    const double frac = frames ? (double) good / frames : 0.0;
    std::printf ("    %s: %d/%d frames within 20 cents (%.2f%%)\n", name, good, frames, 100.0 * frac);
    CHECK (frac >= 0.95);
}
} // namespace

TEST ("pitch: accuracy Small range")  { accuracyForRange (Range::Small,  "Small"); }
TEST ("pitch: accuracy Medium range") { accuracyForRange (Range::Medium, "Medium"); }
TEST ("pitch: accuracy Large range")  { accuracyForRange (Range::Large,  "Large"); }

TEST ("pitch: sine accuracy is within 5 cents on the clean tones")
{
    for (Range r : { Range::Small, Range::Medium, Range::Large })
        for (double sr : { 44100.0, 48000.0 })
            for (double hz : pitchesFor (r))
            {
                const auto s = scoreTone (r, sr, hz, Timbre::Sine);
                if (s.worst > 5.0)
                    std::printf ("    range=%d sr=%.0f hz=%.1f worst %.2f c\n", (int) r, sr, hz, s.worst);
                CHECK (s.worst <= 5.0);
            }
}

TEST ("pitch: hop and window sizes follow the range")
{
    PitchDetector d;
    d.prepare (48000.0, Range::Small);  CHECK (d.windowSamples() == 1024 && d.hopSamples() == 256);
    d.prepare (48000.0, Range::Medium); CHECK (d.windowSamples() == 2048);
    d.prepare (44100.0, Range::Large);  CHECK (d.windowSamples() == 4096);
    CHECK_NEAR (d.hopRateHz(), 44100.0 / 256.0, 1e-6);
}

TEST ("pitch: level is window RMS in dBFS")
{
    PitchDetector d;
    d.prepare (48000.0, Range::Medium);
    auto x = tone (48000.0, 0.5, 440.0, 0.5, Timbre::Sine);
    auto r = runDetector (d, x);
    CHECK_NEAR (r.est.back().levelDb, 20.0 * std::log10 (0.5 / std::sqrt (2.0)), 0.1);
    CHECK (r.est.back().clarity > 0.95f);
}

TEST ("pitch: pitch outside the band is not reported")
{
    PitchDetector d;
    d.prepare (48000.0, Range::Small);          // 130.8 .. 523.3 Hz
    auto x = tone (48000.0, 0.5, 60.0, 0.5, Timbre::Sine);   // below the band: no lag inside it
    auto r = runDetector (d, x);
    int valid = 0;
    for (auto& e : r.est) valid += e.valid ? 1 : 0;
    CHECK (valid == 0);
    for (auto& e : r.est) if (! e.valid) CHECK (e.hz == 0.0f);
}

TEST ("pitch: silence is invalid, white noise yields no notes")
{
    for (Range range : { Range::Small, Range::Medium, Range::Large })
    {
        PitchDetector d;
        d.prepare (48000.0, range);
        NoteTracker t;
        t.prepare (d.hopRateHz());

        std::vector<float> silence (48000, 0.0f);
        auto rs = runDetector (d, silence);
        for (auto& e : rs.est) { CHECK (! e.valid); CHECK (e.hz == 0.0f); CHECK (e.levelDb <= -119.0f); }

        std::uint32_t s = 12345u;
        std::vector<float> noise (48000 * 4);
        for (auto& v : noise) { s ^= s << 13; s ^= s >> 17; s ^= s << 5; v = 0.5f * ((float) (s >> 8) / 8388608.0f - 1.0f); }
        auto rn = runDetector (d, noise);
        int valid = 0, events = 0;
        for (auto& e : rn.est)
        {
            valid += e.valid ? 1 : 0;
            NoteEvent ev[2];
            events += t.process (e, ev);
        }
        if (valid) std::printf ("    range %d: %d noise frames valid\n", (int) range, valid);
        CHECK (valid == 0);
        CHECK (events == 0);
    }
}

TEST ("pitch: cost of MEDIUM at 48k is small (release builds only assert the bound)")
{
    PitchDetector d;
    d.prepare (48000.0, Range::Medium);
    auto x = tone (48000.0, 10.0, 220.0, 0.5, Timbre::Saw);
    Estimate tmp[16];
    const auto t0 = std::chrono::steady_clock::now();
    int n = 0;
    for (size_t pos = 0; pos + 512 <= x.size(); pos += 512) n += d.process (x.data() + pos, 512, tmp, 16);
    const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    std::printf ("    MEDIUM 48k: %.3f s for 10 s of audio (%.2f%% of one core), %d estimates\n", secs, secs * 10.0, n);
    CHECK (n > 1800);
#ifdef NDEBUG
    CHECK (secs < 0.2);   // 2% of real time
#endif
}

// ───────────────────────── note tracker, driven by raw estimates ─────────────────────────

namespace
{
Estimate est (double hz, float clarity = 0.95f, float level = -20.0f)
{
    Estimate e;
    e.valid = hz > 0.0; e.hz = (float) hz; e.clarity = clarity; e.levelDb = level;
    return e;
}
double hzFromMidi (double m) { return 440.0 * std::pow (2.0, (m - 69.0) / 12.0); }
}

TEST ("tracker: On after 3 stable hops")
{
    NoteTracker t; t.prepare (187.5);
    NoteEvent ev[2];
    CHECK (t.process (est (440.0), ev) == 0);
    CHECK (t.process (est (440.5), ev) == 0);
    CHECK (t.process (est (439.8), ev) == 1);
    CHECK (ev[0].type == NoteEvent::Type::On);
    CHECK (ev[0].midiNote == 69);
    CHECK_NEAR (ev[0].hz, 440.0, 1.0);
    CHECK (std::fabs (ev[0].cents) < 5.0f);
    CHECK (t.isHeld() && t.currentNote() == 69);
    for (int i = 0; i < 50; ++i) CHECK (t.process (est (440.0), ev) == 0);
}

TEST ("tracker: cents reported relative to the nearest semitone")
{
    NoteTracker t; t.prepare (187.5);
    NoteEvent ev[2];
    int n = 0;
    for (int i = 0; i < 3; ++i) n = t.process (est (hzFromMidi (60.3)), ev);
    CHECK (n == 1);
    CHECK (ev[0].midiNote == 60);
    CHECK_NEAR (ev[0].cents, 30.0, 1.0);
}

TEST ("tracker: unstable estimates do not start a note")
{
    NoteTracker t; t.prepare (187.5);
    NoteEvent ev[2];
    int events = 0;
    for (int i = 0; i < 40; ++i)
        events += t.process (est (hzFromMidi (60 + (i % 2 ? 0 : 3))), ev);     // jumping about
    CHECK (events == 0);
    // Low clarity: never starts
    for (int i = 0; i < 40; ++i) events += t.process (est (440.0, 0.7f), ev);
    CHECK (events == 0);
    // Invalid estimates break a streak
    t.process (est (440.0), ev); t.process (est (440.0), ev);
    t.process (est (0.0, 0.0f), ev);
    CHECK (t.process (est (440.0), ev) == 0);
}

TEST ("tracker: Off on silence")
{
    NoteTracker t; t.prepare (187.5);
    NoteEvent ev[2];
    for (int i = 0; i < 3; ++i) t.process (est (440.0), ev);
    CHECK (t.isHeld());
    const int n = t.process (est (0.0, 0.0f, -90.0f), ev);
    CHECK (n == 1 && ev[0].type == NoteEvent::Type::Off && ev[0].midiNote == 69);
    CHECK (! t.isHeld());
    // and nothing more
    CHECK (t.process (est (0.0, 0.0f, -90.0f), ev) == 0);
}

TEST ("tracker: Off after 3 hops of low clarity, but not before")
{
    NoteTracker t; t.prepare (187.5);
    NoteEvent ev[2];
    for (int i = 0; i < 3; ++i) t.process (est (440.0), ev);
    CHECK (t.process (est (440.0, 0.3f), ev) == 0);
    CHECK (t.process (est (440.0, 0.3f), ev) == 0);
    CHECK (t.process (est (440.0, 0.95f), ev) == 0);    // recovers: counter resets
    CHECK (t.process (est (440.0, 0.3f), ev) == 0);
    CHECK (t.process (est (440.0, 0.3f), ev) == 0);
    CHECK (t.process (est (440.0, 0.3f), ev) == 1);
    CHECK (ev[0].type == NoteEvent::Type::Off);
}

TEST ("tracker: clarity between 0.5 and 0.8 keeps a held note but cannot start one")
{
    NoteTracker t; t.prepare (187.5);
    NoteEvent ev[2];
    for (int i = 0; i < 10; ++i) CHECK (t.process (est (440.0, 0.65f), ev) == 0);
    for (int i = 0; i < 3; ++i) t.process (est (440.0, 0.9f), ev);
    CHECK (t.isHeld());
    for (int i = 0; i < 30; ++i) CHECK (t.process (est (440.0, 0.65f), ev) == 0);
    CHECK (t.isHeld());
}

TEST ("tracker: semitone jump gives Off then On in one call")
{
    NoteTracker t; t.prepare (187.5);
    NoteEvent ev[2];
    for (int i = 0; i < 3; ++i) t.process (est (440.0), ev);
    CHECK (t.process (est (466.16), ev) == 0);
    CHECK (t.process (est (466.16), ev) == 0);
    const int n = t.process (est (466.16), ev);
    CHECK (n == 2);
    CHECK (ev[0].type == NoteEvent::Type::Off && ev[0].midiNote == 69);
    CHECK (ev[1].type == NoteEvent::Type::On && ev[1].midiNote == 70);
    CHECK (t.currentNote() == 70);
}

TEST ("tracker: a 20 cent step and a slow glide under 35 cents do not retrigger")
{
    NoteTracker t; t.prepare (187.5);
    NoteEvent ev[2];
    for (int i = 0; i < 3; ++i) t.process (est (440.0), ev);
    int events = 0;
    for (int i = 0; i < 30; ++i) events += t.process (est (440.0 * std::pow (2.0, 20.0 / 1200.0)), ev);
    for (int i = 0; i < 300; ++i)
        events += t.process (est (440.0 * std::pow (2.0, (30.0 * i / 300.0) / 1200.0)), ev);
    CHECK (events == 0);
    CHECK (t.currentNote() == 69);
}

TEST ("tracker: a long glide up a tone steps through semitones")
{
    NoteTracker t; t.prepare (187.5);
    NoteEvent ev[2];
    int ons = 0, offs = 0, last = -1;
    for (int i = 0; i < 3; ++i) t.process (est (440.0), ev);
    for (int i = 0; i < 560; ++i)   // 3 s at 187.5 Hz would be 562 hops: 2 semitones in 3 s
    {
        const double cents = 200.0 * i / 560.0;
        const int n = t.process (est (440.0 * std::pow (2.0, cents / 1200.0)), ev);
        for (int k = 0; k < n; ++k) { (ev[k].type == NoteEvent::Type::On ? ons : offs)++; if (ev[k].type == NoteEvent::Type::On) last = ev[k].midiNote; }
    }
    CHECK (ons >= 1 && ons == offs);
    CHECK (last >= 70 && last <= 71);
}

TEST ("tracker: vibrato of +-30 cents does not retrigger")
{
    for (double centre : { 69.0, 69.45, 68.6 })
        for (double startPhase : { 0.0, 1.5708, 3.1416 * 1.5 })
        {
            NoteTracker t; t.prepare (187.5);
            NoteEvent ev[2];
            int ons = 0, offs = 0;
            for (int i = 0; i < 1500; ++i)
            {
                const double cents = 30.0 * std::sin (startPhase + 2.0 * kPi * 5.5 * i / 187.5);
                const int n = t.process (est (hzFromMidi (centre + cents / 100.0)), ev);
                for (int k = 0; k < n; ++k) (ev[k].type == NoteEvent::Type::On ? ons : offs)++;
            }
            if (ons != 1 || offs != 0)
                std::printf ("    centre %.2f phase %.2f: %d On, %d Off\n", centre, startPhase, ons, offs);
            CHECK (ons == 1);
            CHECK (offs == 0);
        }
}

TEST ("tracker: gate threshold and hysteresis")
{
    NoteTracker t; t.prepare (187.5);
    t.setGateDb (-40.0f);
    NoteEvent ev[2];
    int events = 0;
    for (int i = 0; i < 20; ++i) events += t.process (est (440.0, 0.95f, -41.0f), ev);
    CHECK (events == 0);
    for (int i = 0; i < 3; ++i) events += t.process (est (440.0, 0.95f, -39.0f), ev);
    CHECK (events == 1);
    // between gate and gate-3 dB: still held
    for (int i = 0; i < 10; ++i) CHECK (t.process (est (440.0, 0.95f, -42.0f), ev) == 0);
    CHECK (t.isHeld());
    // below gate-3 dB: Off
    CHECK (t.process (est (440.0, 0.95f, -44.0f), ev) == 1);
    CHECK (ev[0].type == NoteEvent::Type::Off);
    // a new note needs the full gate again
    for (int i = 0; i < 10; ++i) CHECK (t.process (est (440.0, 0.95f, -41.0f), ev) == 0);
}

TEST ("tracker: reset clears the held note")
{
    NoteTracker t; t.prepare (187.5);
    NoteEvent ev[2];
    for (int i = 0; i < 3; ++i) t.process (est (440.0), ev);
    t.reset();
    CHECK (! t.isHeld());
    CHECK (t.process (est (440.0), ev) == 0);
}

// ───────────────────────── end to end: audio -> detector -> tracker ─────────────────────────

namespace
{
struct Events { std::vector<std::pair<size_t, NoteEvent>> list; };

Events runChain (Range range, double sr, const std::vector<float>& x, float gateDb = -58.0f)
{
    PitchDetector d; d.prepare (sr, range);
    NoteTracker t; t.prepare (d.hopRateHz()); t.setGateDb (gateDb);
    Events out;
    Estimate tmp[16];
    size_t hopIndex = 0;
    for (size_t pos = 0; pos < x.size(); pos += 256)
    {
        const int n = (int) std::min ((size_t) 256, x.size() - pos);
        const int got = d.process (x.data() + pos, n, tmp, 16);
        for (int i = 0; i < got; ++i, ++hopIndex)
        {
            NoteEvent ev[2];
            const int ne = t.process (tmp[i], ev);
            for (int k = 0; k < ne; ++k) out.list.push_back ({ pos + (size_t) n, ev[k] });
        }
    }
    return out;
}
}

TEST ("chain: note on after onset, off after silence, within ~45 ms at MEDIUM")
{
    const double sr = 48000.0;
    std::vector<float> x ((size_t) (sr * 1.5), 0.0f);
    double ph = 0.0;
    const size_t on = (size_t) (0.3 * sr), off = (size_t) (0.9 * sr);
    addTone (x, on, off, sr, 220.0, 0.4, Timbre::Saw, ph);
    auto ev = runChain (Range::Medium, sr, x);
    CHECK (ev.list.size() == 2);
    if (ev.list.size() == 2)
    {
        CHECK (ev.list[0].second.type == NoteEvent::Type::On && ev.list[0].second.midiNote == 57);
        CHECK (ev.list[1].second.type == NoteEvent::Type::Off && ev.list[1].second.midiNote == 57);
        const double latMs = 1000.0 * ((double) ev.list[0].first - (double) on) / sr;
        const double offMs = 1000.0 * ((double) ev.list[1].first - (double) off) / sr;
        std::printf ("    On latency %.1f ms, Off latency %.1f ms\n", latMs, offMs);
        CHECK (latMs < 60.0);
        CHECK (offMs < 90.0);
    }
}

TEST ("chain: semitone jump in audio gives Off+On, glide inside 35 cents does not")
{
    const double sr = 44100.0;
    std::vector<float> x ((size_t) (sr * 2.0), 0.0f);
    double ph = 0.0;
    const size_t half = x.size() / 2;
    addTone (x, 0, half, sr, 330.0, 0.4, Timbre::Sine, ph);
    addTone (x, half, x.size(), sr, 330.0 * std::pow (2.0, 1.0 / 12.0), 0.4, Timbre::Sine, ph);
    auto ev = runChain (Range::Medium, sr, x);
    CHECK (ev.list.size() == 3);
    if (ev.list.size() == 3)
    {
        CHECK (ev.list[0].second.type == NoteEvent::Type::On && ev.list[0].second.midiNote == 64);
        CHECK (ev.list[1].second.type == NoteEvent::Type::Off && ev.list[1].second.midiNote == 64);
        CHECK (ev.list[2].second.type == NoteEvent::Type::On && ev.list[2].second.midiNote == 65);
    }

    // 30 cent glide
    std::vector<float> g ((size_t) (sr * 2.0), 0.0f);
    double p = 0.0;
    for (size_t i = 0; i < g.size(); ++i)
    {
        const double hz = 330.0 * std::pow (2.0, (30.0 * (double) i / (double) g.size()) / 1200.0);
        g[i] = 0.4f * (float) std::sin (p);
        p += 2.0 * kPi * hz / sr;
    }
    auto gv = runChain (Range::Medium, sr, g);
    CHECK (gv.list.size() == 1);
}

TEST ("chain: +-30 cent vibrato on audio holds one note")
{
    const double sr = 48000.0;
    for (Range range : { Range::Small, Range::Medium, Range::Large })
    {
        std::vector<float> g ((size_t) (sr * 3.0), 0.0f);
        double p = 0.0;
        for (size_t i = 0; i < g.size(); ++i)
        {
            const double cents = 30.0 * std::sin (2.0 * kPi * 5.5 * (double) i / sr);
            g[i] = 0.4f * (float) std::sin (p);
            p += 2.0 * kPi * 261.63 * std::pow (2.0, cents / 1200.0) / sr;
        }
        auto ev = runChain (range, sr, g);
        CHECK (ev.list.size() == 1);
    }
}

TEST ("chain: gate honoured on audio")
{
    const double sr = 48000.0;
    // amplitude 0.002 -> -57 dBFS RMS (about)
    auto quiet = tone (sr, 1.0, 330.0, 0.002, Timbre::Sine);
    CHECK (runChain (Range::Medium, sr, quiet, -40.0f).list.empty());
    auto ev = runChain (Range::Medium, sr, quiet, -66.0f);
    CHECK (! ev.list.empty());
    auto loud = tone (sr, 1.0, 330.0, 0.1, Timbre::Sine);
    CHECK (! runChain (Range::Medium, sr, loud, -40.0f).list.empty());
}
