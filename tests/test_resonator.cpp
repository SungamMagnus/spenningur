#include "test.h"
#include "spenningur/Resonator.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

using spn::Limiter;
using spn::Resonator;
using spn::ResonatorParams;

namespace
{
const double kPi = 3.14159265358979323846;

struct Rng
{
    uint32_t s = 12345u;
    float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (int32_t) s / 2147483648.0f; }   // [-1, 1)
};

struct Out { std::vector<float> l, r; };

// Runs the resonator over `in` in blocks of `block` samples.
Out run (Resonator& res, const std::vector<float>& in, int block)
{
    Out o;
    o.l.assign (in.size(), 0.0f);
    o.r.assign (in.size(), 0.0f);
    for (size_t pos = 0; pos < in.size(); pos += (size_t) block)
    {
        const int n = (int) std::min ((size_t) block, in.size() - pos);
        res.process (in.data() + pos, o.l.data() + pos, o.r.data() + pos, n);
    }
    return o;
}

// Response to the input alone. The internal excitation is part of the ADSR now, so a pure-ring measurement
// subtracts a second run with the same internal noise and silent input (the bank is linear).
Out runIsolated (double sr, const ResonatorParams& p, const int* notes, int count, const std::vector<float>& in)
{
    Resonator a, b;
    for (Resonator* r : { &a, &b })
    {
        r->prepare (sr, 512);
        r->setParams (p);
        r->setChord (notes, count);
        r->noteOn();
    }
    Out oa = run (a, in, 512);
    const Out ob = run (b, std::vector<float> (in.size(), 0.0f), 512);
    for (size_t i = 0; i < oa.l.size(); ++i) { oa.l[i] -= ob.l[i]; oa.r[i] -= ob.r[i]; }
    return oa;
}

ResonatorParams pluckParams (float ring)
{
    ResonatorParams p;
    p.ringSec = ring;
    p.dampHz = 6500.0f;
    p.spreadOct = 0.0f;
    p.glideMs = 0.0f;
    p.attackMs = 1.0f;
    p.decayMs = 10.0f;
    p.sustain = 1.0f;          // envelope fully open: these tests measure the resonator, not the VCA
    p.releaseMs = 2000.0f;
    return p;
}

double hz (int note) { return 440.0 * std::pow (2.0, (note - 69) / 12.0); }

double rms (const std::vector<float>& x, double sr, double t0, double t1)
{
    const size_t a = (size_t) (t0 * sr), b = std::min (x.size(), (size_t) (t1 * sr));
    double s = 0.0;
    for (size_t i = a; i < b; ++i) s += (double) x[i] * x[i];
    return std::sqrt (s / (double) std::max<size_t> (1, b - a));
}

double peakAbs (const std::vector<float>& x, size_t a = 0, size_t b = (size_t) -1)
{
    b = std::min (b, x.size());
    double p = 0.0;
    for (size_t i = a; i < b; ++i) p = std::max (p, (double) std::fabs (x[i]));
    return p;
}

bool allFinite (const std::vector<float>& x)
{
    for (float v : x) if (! std::isfinite (v)) return false;
    return true;
}

// Frequency of the spectral peak of x[a..b) near f0 (Hann-windowed single-bin scan, parabolic refinement).
double peakHz (const std::vector<float>& x, double sr, double f0, size_t a, size_t b)
{
    const size_t n = b - a;
    std::vector<double> win (n);
    for (size_t i = 0; i < n; ++i) win[i] = 0.5 - 0.5 * std::cos (2.0 * kPi * (double) i / (double) n);
    auto mag = [&] (double f)
    {
        const double w = 2.0 * kPi * f / sr;
        const double cw = std::cos (w), sw = std::sin (w);
        double c = 1.0, s = 0.0, re = 0.0, im = 0.0;
        for (size_t i = 0; i < n; ++i)
        {
            const double v = (double) x[a + i] * win[i];
            re += v * c; im -= v * s;
            const double c2 = c * cw - s * sw;
            s = s * cw + c * sw; c = c2;
        }
        return std::sqrt (re * re + im * im);
    };
    const double step = f0 * 0.0005;
    int best = 0;
    double bv = -1.0;
    std::vector<double> m;
    for (int k = -80; k <= 80; ++k) { m.push_back (mag (f0 + k * step)); if (m.back() > bv) { bv = m.back(); best = k + 80; } }
    best = std::clamp (best, 1, 159);
    const double y0 = m[(size_t) best - 1], y1 = m[(size_t) best], y2 = m[(size_t) best + 1];
    const double den = y0 - 2.0 * y1 + y2;
    const double frac = den != 0.0 ? 0.5 * (y0 - y2) / den : 0.0;
    return f0 + ((double) (best - 80) + frac) * step;
}

double cents (double f, double ref) { return 1200.0 * std::log2 (f / ref); }

// Amplitude of the component at f in x[a..b) (single-bin DFT).
double binAmp (const std::vector<float>& x, double sr, double f, size_t a, size_t b)
{
    double re = 0.0, im = 0.0;
    const double w = 2.0 * kPi * f / sr;
    for (size_t i = a; i < b; ++i)
    {
        re += (double) x[i] * std::cos (w * (double) (i - a));
        im -= (double) x[i] * std::sin (w * (double) (i - a));
    }
    return 2.0 * std::sqrt (re * re + im * im) / (double) (b - a);
}

std::vector<float> impulse (size_t n, float amp = 1.0f)
{
    std::vector<float> v (n, 0.0f);
    v[0] = amp;
    return v;
}

std::vector<float> noise (size_t n, float amp, uint32_t seed = 777u)
{
    Rng r; r.s = seed;
    std::vector<float> v (n);
    for (auto& s : v) s = amp * r.next();
    return v;
}

double maxJump (const std::vector<float>& x, size_t a, size_t b)
{
    double m = 0.0;
    for (size_t i = std::max<size_t> (a, 1); i < std::min (b, x.size()); ++i)
        m = std::max (m, (double) std::fabs (x[i] - x[i - 1]));
    return m;
}
} // namespace

TEST ("resonator: impulse rings at the right pitch (44.1 / 48 / 96 kHz)")
{
    const double rates[] = { 44100.0, 48000.0, 96000.0 };
    const int notes[] = { 36, 45, 57, 69, 81, 93 };
    double worst = 0.0;
    for (double sr : rates)
        for (int note : notes)
        {
            const auto o = runIsolated (sr, pluckParams (1.5f), &note, 1, impulse ((size_t) (0.8 * sr)));
            const double f0 = hz (note);
            const double est = peakHz (o.l, sr, f0, (size_t) (0.1 * sr), (size_t) (0.6 * sr));
            const double c = cents (est, f0);
            worst = std::max (worst, std::fabs (c));
            if (std::fabs (c) > 10.0) std::printf ("    note %d @ %.0f Hz: %.2f cents\n", note, sr, c);
            CHECK (std::fabs (c) < 10.0);
        }
    std::printf ("    worst pitch error: %.3f cents\n", worst);
}

TEST ("resonator: T60 matches RING")
{
    const double sr = 48000.0;
    const float rings[] = { 0.5f, 1.0f, 2.0f, 4.0f };
    const int notes[] = { 48, 57, 69 };
    double worst = 0.0;
    for (float ring : rings)
        for (int note : notes)
        {
            const double span = std::min (0.8 * ring, 2.0);
            const auto o = runIsolated (sr, pluckParams (ring), &note, 1, impulse ((size_t) ((0.2 + span + 0.1) * sr)));
            const double f0 = hz (note);
            // least-squares slope of ln(amplitude of the fundamental) over time
            double sx = 0, sy = 0, sxx = 0, sxy = 0; int cnt = 0;
            const double win = 0.05;
            for (double t = 0.15; t + win <= 0.2 + span; t += 0.05)
            {
                const double a = binAmp (o.l, sr, f0, (size_t) (t * sr), (size_t) ((t + win) * sr));
                const double lt = t + win / 2, la = std::log (a);
                sx += lt; sy += la; sxx += lt * lt; sxy += lt * la; ++cnt;
            }
            const double slope = (cnt * sxy - sx * sy) / (cnt * sxx - sx * sx);
            const double t60 = -6.907755 / slope;
            const double err = t60 / ring - 1.0;
            worst = std::max (worst, std::fabs (err));
            if (std::fabs (err) > 0.15) std::printf ("    ring %.1f note %d: T60 %.3f (%.1f%%)\n", ring, note, t60, err * 100.0);
            CHECK (std::fabs (err) < 0.15);
        }
    std::printf ("    worst T60 error: %.2f%%\n", worst * 100.0);
}

TEST ("resonator: stable for 20 s of full-scale noise at RING 8 s, 6 voices")
{
    const double rates[] = { 48000.0, 44100.0 };
    for (double sr : rates)
    {
        Resonator r;
        r.prepare (sr, 512);
        ResonatorParams p;
        p.ringSec = 8.0f; p.sustain = 1.0f; p.spreadOct = 1.2f; p.dampHz = 12000.0f;
        r.setParams (p);
        const int chord[] = { 36, 48, 60, 72, 84, 96 };
        r.setChord (chord, 6);
        r.noteOn();
        double peak = 0.0;
        bool finite = true;
        const size_t total = (size_t) (20.0 * sr), chunk = 48000;
        Rng rng;
        for (size_t done = 0; done < total; done += chunk)
        {
            std::vector<float> in (chunk);
            for (auto& s : in) s = rng.next();                 // full-scale uniform noise
            const auto o = run (r, in, 512);
            finite = finite && allFinite (o.l) && allFinite (o.r);
            peak = std::max ({ peak, peakAbs (o.l), peakAbs (o.r) });
        }
        std::printf ("    %.0f Hz: peak %.3f\n", sr, peak);
        CHECK (finite);
        CHECK (peak < 8.0);
    }
    // full-scale DC and full-scale square at a chord frequency must not run away either
    Resonator r;
    r.prepare (48000.0, 512);
    ResonatorParams p; p.ringSec = 8.0f; p.sustain = 1.0f;
    r.setParams (p);
    const int chord[] = { 45, 52, 57 };
    r.setChord (chord, 3);
    r.noteOn();
    std::vector<float> dc ((size_t) (10 * 48000), 1.0f);
    auto o = run (r, dc, 512);
    CHECK (allFinite (o.l));
    CHECK (peakAbs (o.l) < 8.0);
    std::vector<float> sq ((size_t) (10 * 48000));
    for (size_t i = 0; i < sq.size(); ++i) sq[i] = std::fmod ((double) i * 220.0 / 48000.0, 1.0) < 0.5 ? 1.0f : -1.0f;
    o = run (r, sq, 512);
    std::printf ("    full-scale 220 Hz square on A: peak %.3f\n", peakAbs (o.l));
    CHECK (allFinite (o.l));
    CHECK (peakAbs (o.l) < 8.0);
}

TEST ("resonator: no denormals or NaN while decaying to silence")
{
    const double sr = 48000.0;
    Resonator r;
    r.prepare (sr, 512);
    ResonatorParams p; p.ringSec = 8.0f; p.releaseMs = 20.0f;
    r.setParams (p);
    const int chord[] = { 48, 55, 60, 64 };
    r.setChord (chord, 4);
    r.noteOn();
    run (r, noise ((size_t) (0.5 * sr), 1.0f), 512);
    r.noteOff();
    long sub = 0;
    bool finite = true;
    for (int sec = 0; sec < 40; ++sec)
    {
        const auto o = run (r, std::vector<float> ((size_t) sr, 0.0f), 512);
        for (size_t i = 0; i < o.l.size(); ++i)
        {
            if (std::fpclassify (o.l[i]) == FP_SUBNORMAL || std::fpclassify (o.r[i]) == FP_SUBNORMAL) ++sub;
            if (! std::isfinite (o.l[i]) || ! std::isfinite (o.r[i])) finite = false;
        }
    }
    CHECK (sub == 0);
    CHECK (finite);
}

TEST ("resonator: chord change at GLIDE 0 does not click")
{
    const double sr = 48000.0;
    Resonator r;
    r.prepare (sr, 512);
    ResonatorParams p; p.glideMs = 0.0f; p.ringSec = 4.0f; p.sustain = 0.0f; p.attackMs = 1.0f; p.spreadOct = 0.0f;
    r.setParams (p);
    const int c1[] = { 48, 52, 55 };
    const int c2[] = { 53, 57, 60, 65 };       // also changes the voice count
    r.setChord (c1, 3);
    r.noteOn();
    run (r, noise ((size_t) sr, 0.3f), 512);                       // ring up the first chord

    auto o1 = run (r, std::vector<float> ((size_t) (0.3 * sr), 0.0f), 512);
    const double ref = maxJump (o1.l, 0, o1.l.size());
    r.setChord (c2, 4);                                            // setChord does not retrigger the envelope
    auto o2 = run (r, noise ((size_t) (0.5 * sr), 0.02f, 5u), 512);
    // include the seam between the two buffers
    std::vector<float> seam (o1.l.end() - 8, o1.l.end());
    seam.insert (seam.end(), o2.l.begin(), o2.l.end());
    const double after = maxJump (seam, 0, seam.size());
    std::printf("    max jump before %.5f, around change %.5f\n", ref, after);
    CHECK (allFinite (o2.l));
    CHECK (after < 2.0 * ref + 0.002);

    // and back to fewer voices
    const int c3[] = { 50 };
    r.setChord (c3, 1);
    auto o3 = run (r, std::vector<float> ((size_t) (0.5 * sr), 0.0f), 512);
    CHECK (maxJump (o3.l, 0, o3.l.size()) < 2.0 * ref + 0.002);
}

TEST ("resonator: GLIDE 100 ms retunes smoothly and lands in tune")
{
    const double sr = 48000.0;
    Resonator r;
    r.prepare (sr, 512);
    ResonatorParams p = pluckParams (2.0f);
    p.glideMs = 100.0f;
    p.sustain = 1.0f;
    r.setParams (p);
    int n1 = 57, n2 = 64;
    r.setChord (&n1, 1);
    r.noteOn();
    run (r, noise ((size_t) (0.5 * sr), 0.05f), 512);
    const auto ref = run (r, noise ((size_t) (0.2 * sr), 0.05f, 9u), 512);
    r.setChord (&n2, 1);
    const auto o = run (r, noise ((size_t) (1.5 * sr), 0.05f, 11u), 512);
    CHECK (allFinite (o.l));
    CHECK (maxJump (o.l, 0, o.l.size()) < 2.5 * maxJump (ref.l, 0, ref.l.size()) + 0.01);
    const double est = peakHz (o.l, sr, hz (n2), (size_t) (0.8 * sr), (size_t) (1.5 * sr));
    CHECK (std::fabs (cents (est, hz (n2))) < 15.0);
}

TEST ("resonator: transient setting dies quickly, pad swells and holds")
{
    const double sr = 48000.0;
    const int chord[] = { 48, 55, 60, 64 };
    const size_t total = (size_t) (8.0 * sr);

    // transient: sustain 0, short release, short ring
    {
        Resonator r;
        r.prepare (sr, 512);
        ResonatorParams p; p.attackMs = 1.0f; p.decayMs = 150.0f; p.sustain = 0.0f; p.releaseMs = 200.0f; p.ringSec = 0.3f;
        r.setParams (p);
        r.setChord (chord, 4);
        r.noteOn();
        const auto o = run (r, impulse (total), 512);
        const double early = rms (o.l, sr, 0.0, 0.1), late = rms (o.l, sr, 1.0, 1.2);
        std::printf ("    transient: rms early %.5f late %.3g\n", early, late);
        CHECK (early > 0.005);
        CHECK (late < early * 0.003);
    }
    // the ADSR is the output VCA: with sustain 0 even a *held* input (continuous noise) dies away on DECAY
    {
        Resonator r;
        r.prepare (sr, 512);
        ResonatorParams p; p.attackMs = 1.0f; p.decayMs = 150.0f; p.sustain = 0.0f; p.ringSec = 4.0f;
        r.setParams (p);
        r.setChord (chord, 4);
        r.noteOn();
        const auto o = run (r, noise ((size_t) (3.0 * sr), 0.1f * 1.7320508f), 512);
        const double a = rms (o.l, sr, 0.0, 0.1), b = rms (o.l, sr, 2.0, 2.5);
        std::printf ("    sustain 0, held noise: rms early %.5f, late %.3g\n", a, b);
        CHECK (b < a * 0.01);
    }
    // sustain 1 with the same held noise keeps sounding
    {
        Resonator r;
        r.prepare (sr, 512);
        ResonatorParams p; p.attackMs = 1.0f; p.decayMs = 150.0f; p.sustain = 1.0f; p.ringSec = 4.0f;
        r.setParams (p);
        r.setChord (chord, 4);
        r.noteOn();
        const auto o = run (r, noise ((size_t) (3.0 * sr), 0.1f * 1.7320508f), 512);
        const double b = rms (o.l, sr, 2.0, 2.5);
        CHECK (b > 0.005);
    }
    // pad, held: single click
    double padLate = 0.0, padPeak = 0.0;
    {
        Resonator r;
        r.prepare (sr, 512);
        ResonatorParams p; p.attackMs = 600.0f; p.decayMs = 400.0f; p.sustain = 0.8f; p.releaseMs = 1000.0f; p.ringSec = 4.5f;
        r.setParams (p);
        r.setChord (chord, 4);
        r.noteOn();
        const size_t offAt = (size_t) (5.0 * sr);
        auto in = impulse (offAt);
        auto o = run (r, in, 512);
        const double r0 = rms (o.l, sr, 0.0, 0.05), r1 = rms (o.l, sr, 0.5, 0.65);
        padLate = rms (o.l, sr, 3.0, 3.2);
        padPeak = peakAbs (o.l);
        const double held = rms (o.l, sr, 4.5, 5.0);
        std::printf ("    pad: rms 0-50ms %.5f, 500-650ms %.5f, 3.0-3.2s %.5f, 4.5-5s %.5f (env %.2f)\n", r0, r1, padLate, held, r.envelopeLevel());
        CHECK (r1 > r0 * 4.0);                 // swells
        CHECK (padLate > 0.01);                // still clearly sounding 3 s after a single click
        CHECK (held > 0.01);
        CHECK (r.envelopeLevel() > 0.7 && r.envelopeLevel() < 0.9);
        r.noteOff();
        o = run (r, std::vector<float> ((size_t) (3.0 * sr), 0.0f), 512);
        const double t1 = rms (o.l, sr, 0.0, 0.05), t2 = rms (o.l, sr, 1.1, 1.3), t3 = rms (o.l, sr, 2.0, 2.2);
        std::printf ("    pad release (1 s): rms just after off %.5f, +1.1 s %.5f, +2 s %.5f\n", t1, t2, t3);
        CHECK (t2 < t1 * 0.02);                // -34 dB after releaseMs (+10%)
        CHECK (t3 < t1 * 0.001);
        CHECK (r.envelopeLevel() < 0.01);
    }
    // the same click without internal sustain: much quieter at 3 s, so the pad really is the excitation
    {
        Resonator r;
        r.prepare (sr, 512);
        ResonatorParams p; p.attackMs = 600.0f; p.decayMs = 400.0f; p.sustain = 0.0f; p.ringSec = 4.5f;
        r.setParams (p);
        r.setChord (chord, 4);
        r.noteOn();
        const auto o = run (r, impulse ((size_t) (5.0 * sr)), 512);
        const double late = rms (o.l, sr, 3.0, 3.2);
        std::printf ("    same click, sustain 0: rms 3.0-3.2s %.5f\n", late);
        CHECK (padLate > 3.0 * late);
    }
    // level reference: a normal input-excited chord. Continuous noise at -20 dBFS rms (a sustained played note) into the
    // same chord, measured once it has built up; the full-scale click is shown for information (it is a very weak
    // excitation: one sample of energy).
    {
        Resonator r;
        r.prepare (sr, 512);
        ResonatorParams p; p.attackMs = 1.0f; p.decayMs = 10.0f; p.sustain = 1.0f; p.ringSec = 4.5f;
        r.setParams (p);
        r.setChord (chord, 4);
        r.noteOn();
        const auto o = run (r, noise ((size_t) (4.0 * sr), 0.1f * 1.7320508f), 512);     // uniform noise with 0.1 rms
        const double refBurst = rms (o.l, sr, 3.0, 4.0);

        Resonator r2;
        r2.prepare (sr, 512);
        r2.setParams (p);
        r2.setChord (chord, 4);
        r2.noteOn();
        const auto o2 = run (r2, impulse ((size_t) (1.0 * sr)), 512);
        const double refClick = rms (o2.l, sr, 0.0, 0.5);
        std::printf ("    reference chords: -20 dBFS noise rms %.5f, click rms %.5f ; pad held rms %.5f (%.1f dB re noise, %.1f dB re click) ; pad peak %.3f\n",
                     refBurst, refClick, padLate, 20 * std::log10 (padLate / refBurst), 20 * std::log10 (padLate / refClick), padPeak);
        CHECK (padLate <= refBurst);
        CHECK (padLate > 0.5 * refBurst);
        CHECK (padLate > 0.01);
    }
}

TEST ("resonator: stereo spread")
{
    const double sr = 48000.0;
    const int chord[] = { 48, 52, 55, 60 };
    const auto in = noise ((size_t) sr, 0.2f);
    double diff0 = 0.0, diff1 = 0.0;
    for (int pass = 0; pass < 2; ++pass)
    {
        Resonator r;
        r.prepare (sr, 512);
        ResonatorParams p; p.sustain = 1.0f; p.spreadOct = pass == 0 ? 0.0f : 1.5f;
        r.setParams (p);
        r.setChord (chord, 4);
        r.noteOn();
        const auto o = run (r, in, 512);
        double d = 0.0, e = 0.0;
        for (size_t i = (size_t) (0.5 * sr); i < o.l.size(); ++i)
        {
            d += std::pow ((double) o.l[i] - o.r[i], 2.0);
            e += std::pow ((double) o.l[i], 2.0) + std::pow ((double) o.r[i], 2.0);
        }
        (pass == 0 ? diff0 : diff1) = std::sqrt (d / e);
    }
    std::printf ("    relative L-R difference: spread 0 -> %.5f, spread 1.5 -> %.3f\n", diff0, diff1);
    CHECK (diff0 < 1.0e-4);
    CHECK (diff1 > 0.1);
}

TEST ("resonator: SPREAD 3 with extreme notes stays finite and bounded")
{
    const double sr = 44100.0;
    Resonator r;
    r.prepare (sr, 256);
    ResonatorParams p; p.spreadOct = 3.0f; p.ringSec = 8.0f; p.sustain = 1.0f;
    r.setParams (p);
    const int chord[] = { 36, 96, 36, 96, 40, 90 };
    r.setChord (chord, 6);
    r.noteOn();
    auto o = run (r, noise ((size_t) (5 * sr), 1.0f), 256);
    CHECK (allFinite (o.l) && allFinite (o.r));
    CHECK (peakAbs (o.l) < 8.0 && peakAbs (o.r) < 8.0);
    // sweep SPREAD while running
    for (int i = 0; i < 30; ++i)
    {
        p.spreadOct = 3.0f * (float) (i % 7) / 6.0f;
        r.setParams (p);
        o = run (r, noise (4410, 0.5f, (uint32_t) (i + 1)), 256);
        CHECK (allFinite (o.l) && allFinite (o.r));
        CHECK (peakAbs (o.l) < 8.0);
    }
}

TEST ("resonator: in may alias outL")
{
    const double sr = 48000.0;
    const int chord[] = { 48, 55, 64 };
    const auto in = noise ((size_t) (0.5 * sr), 0.3f);
    Resonator a, b;
    for (auto* r : { &a, &b })
    {
        r->prepare (sr, 512);
        ResonatorParams p; p.sustain = 0.5f;
        r->setParams (p);
        r->setChord (chord, 3);
        r->noteOn();
    }
    const auto ref = run (a, in, 512);
    std::vector<float> inplace = in, right (in.size());
    for (size_t pos = 0; pos < in.size(); pos += 512)
    {
        const int n = (int) std::min<size_t> (512, in.size() - pos);
        b.process (inplace.data() + pos, inplace.data() + pos, right.data() + pos, n);
    }
    double d = 0.0;
    for (size_t i = 0; i < in.size(); ++i) d = std::max (d, (double) std::fabs (ref.l[i] - inplace[i]));
    CHECK (d == 0.0);
}

TEST ("resonator: output does not depend on the block size")
{
    const double sr = 48000.0;
    const size_t changeAt = 37u * 512u;                       // a multiple of 1, 37 and 512
    const size_t total = changeAt * 2;
    const auto in = noise (total, 0.25f);
    std::vector<Out> outs;
    for (int block : { 1, 37, 512 })
    {
        Resonator r;
        r.prepare (sr, 512);
        ResonatorParams p; p.glideMs = 40.0f; p.sustain = 0.6f; p.attackMs = 30.0f; p.spreadOct = 1.2f;
        r.setParams (p);
        const int c1[] = { 48, 52, 55 };
        const int c2[] = { 53, 57, 60, 65 };
        r.setChord (c1, 3);
        r.noteOn();
        Out o;
        o.l.resize (total); o.r.resize (total);
        for (size_t pos = 0; pos < total; pos += (size_t) block)
        {
            if (pos == changeAt) { r.setChord (c2, 4); r.noteOn(); }
            // blocks never straddle the change point because it is a multiple of the block size
            const int n = (int) std::min<size_t> ((size_t) block, total - pos);
            r.process (in.data() + pos, o.l.data() + pos, o.r.data() + pos, n);
        }
        outs.push_back (std::move (o));
    }
    double d1 = 0.0, d2 = 0.0;
    for (size_t i = 0; i < total; ++i)
    {
        d1 = std::max (d1, (double) std::fabs (outs[0].l[i] - outs[1].l[i]));
        d2 = std::max (d2, (double) std::fabs (outs[0].r[i] - outs[2].r[i]));
    }
    std::printf ("    max diff block 1 vs 37: %.3g, 1 vs 512: %.3g (peak %.3f)\n", d1, d2, peakAbs (outs[0].l));
    CHECK (d1 < 1.0e-5);
    CHECK (d2 < 1.0e-5);
    CHECK (peakAbs (outs[0].l) > 0.01);
}

TEST ("limiter: keeps +12 dB under the ceiling and passes quiet audio untouched")
{
    const double sr = 48000.0;
    Limiter lim;
    lim.prepare (sr);
    const size_t n = (size_t) sr;
    std::vector<float> l (n), r (n);
    for (size_t i = 0; i < n; ++i) l[i] = r[i] = 3.98f * (float) std::sin (2.0 * kPi * 1000.0 * (double) i / sr);
    for (size_t pos = 0; pos < n; pos += 480) lim.process (l.data() + pos, r.data() + pos, (int) std::min<size_t> (480, n - pos));
    const double settled = peakAbs (l, (size_t) (0.1 * sr));
    std::printf ("    +12 dB sine: settled peak %.3f, whole-run peak %.3f, GR %.2f dB\n", settled, peakAbs (l), lim.gainReductionDb());
    CHECK (settled < 0.95);
    CHECK (settled > 0.8);
    CHECK (peakAbs (l) < 1.0);
    CHECK (lim.gainReductionDb() > 6.0);
    CHECK (std::isfinite (lim.gainReductionDb()));

    // low-frequency full-scale-plus (below the audio band of the voices) is limited too
    for (size_t i = 0; i < n; ++i) l[i] = r[i] = 4.0f * (float) std::sin (2.0 * kPi * 40.0 * (double) i / sr);
    lim.reset();
    for (size_t pos = 0; pos < n; pos += 100) lim.process (l.data() + pos, r.data() + pos, (int) std::min<size_t> (100, n - pos));
    CHECK (peakAbs (l, (size_t) (0.3 * sr)) < 0.95);

    // quiet signal unchanged
    lim.reset();
    std::vector<float> q (n), q0;
    for (size_t i = 0; i < n; ++i) q[i] = 0.1f * (float) std::sin (2.0 * kPi * 440.0 * (double) i / sr);
    q0 = q;
    std::vector<float> qr = q;
    lim.process (q.data(), qr.data(), (int) n);
    CHECK (q == q0);
    CHECK (qr == q0);
    CHECK (lim.gainReductionDb() == 0.0f);

    // recovers after a loud passage (release ~100 ms)
    for (size_t i = 0; i < n; ++i) l[i] = r[i] = (i < n / 10 ? 3.0f : 0.1f) * (float) std::sin (2.0 * kPi * 440.0 * (double) i / sr);
    lim.reset();
    for (size_t pos = 0; pos < n; pos += 480) lim.process (l.data() + pos, r.data() + pos, (int) std::min<size_t> (480, n - pos));
    CHECK_NEAR (l[n - 100], 0.1 * std::sin (2.0 * kPi * 440.0 * (double) (n - 100) / sr), 0.0005);
    CHECK (lim.gainReductionDb() < 0.01f);

    // disabled = passthrough
    lim.setEnabled (false);
    for (size_t i = 0; i < n; ++i) l[i] = r[i] = 3.0f * (float) std::sin (2.0 * kPi * 1000.0 * (double) i / sr);
    const auto l0 = l;
    lim.process (l.data(), r.data(), (int) n);
    CHECK (l == l0);
    CHECK (lim.gainReductionDb() == 0.0f);
}

TEST ("resonator: CPU, 6 voices at 48 kHz")
{
    const double sr = 48000.0;
    Resonator r;
    r.prepare (sr, 512);
    ResonatorParams p; p.ringSec = 6.0f; p.sustain = 0.7f; p.glideMs = 40.0f;
    r.setParams (p);
    const int chord[] = { 48, 52, 55, 59, 62, 66 };
    r.setChord (chord, 6);
    r.noteOn();
    Limiter lim;
    lim.prepare (sr);
    const auto in = noise (512, 0.2f);
    std::vector<float> l (512), rr (512);
    const int blocks = (int) (20.0 * sr / 512.0);           // 20 s of audio
    const auto t0 = std::chrono::steady_clock::now();
    for (int b = 0; b < blocks; ++b)
    {
        if (b % 400 == 0) { const int c[] = { 48 + (b / 400) % 5, 52, 55, 59, 62, 66 }; r.setChord (c, 6); }
        r.process (in.data(), l.data(), rr.data(), 512);
        lim.process (l.data(), rr.data(), 512);
    }
    const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    const double pct = 100.0 * secs / 20.0;
    std::printf ("    CPU: %.3f%% of one core (bank + limiter)%s\n", pct,
#ifdef __OPTIMIZE__
                 "");
    CHECK (pct < 2.0);
#else
                 " [unoptimised build, not checked]");
#endif
}
