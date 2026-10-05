// McLeod Pitch Method detector and note tracker. In-house implementation.
//
// Detector outline (per 256-sample hop):
//   1. The input is low-passed and decimated to about 12 kHz on the fly (windowed-sinc FIR).
//   2. The NSDF is computed on the decimated window, only for lags inside the band.
//   3. Key maxima are picked MPM-style: the first one above 0.93 x the highest.
//   4. The coarse lag is refined on the full-rate window (NSDF at a few integer lags around
//      it, then parabolic interpolation), which keeps accuracy at a few cents even for
//      high pitches where the decimated lag is only a handful of samples.
#include "spenningur/Pitch.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace spn
{
namespace
{
constexpr float kPeakThreshold = 0.93f;  // key maximum selection, relative to the highest
constexpr float kMinPeak       = 0.4f;   // below this a peak is treated as noise
constexpr double kTargetRate   = 12000.0;
constexpr int kFirTaps         = 95;

inline float dotProduct (const float* a, const float* b, int n)
{
    float s0 = 0, s1 = 0, s2 = 0, s3 = 0, s4 = 0, s5 = 0, s6 = 0, s7 = 0;
    int i = 0;
    for (; i + 8 <= n; i += 8)
    {
        s0 += a[i] * b[i];         s1 += a[i + 1] * b[i + 1];
        s2 += a[i + 2] * b[i + 2]; s3 += a[i + 3] * b[i + 3];
        s4 += a[i + 4] * b[i + 4]; s5 += a[i + 5] * b[i + 5];
        s6 += a[i + 6] * b[i + 6]; s7 += a[i + 7] * b[i + 7];
    }
    for (; i < n; ++i) s0 += a[i] * b[i];
    return ((s0 + s1) + (s2 + s3)) + ((s4 + s5) + (s6 + s7));
}

// Parabolic interpolation through three samples around a maximum at index 1.
inline void parabolic (float a, float b, float c, float& offset, float& peak)
{
    const float den = a - 2.0f * b + c;
    if (den >= -1e-12f) { offset = 0.0f; peak = b; return; }
    offset = 0.5f * (a - c) / den;
    offset = std::max (-1.0f, std::min (1.0f, offset));
    peak = b - 0.25f * (a - c) * offset;
}
} // namespace

// ───────────────────────────── PitchDetector ─────────────────────────────

void PitchDetector::prepare (double sampleRate, Range range)
{
    sr = sampleRate > 1000.0 ? sampleRate : 48000.0;

    int baseWindow = range == Range::Small ? 1024 : range == Range::Medium ? 2048 : 4096;
    minHz = range == Range::Small ? 130.8f : range == Range::Medium ? 82.4f : 30.9f;
    maxHz = range == Range::Small ? 523.3f : range == Range::Medium ? 1318.5f : 2093.0f;
    hop = 256;

    // Windows are defined in samples at 44.1/48 kHz; keep the time span above that.
    window = baseWindow * std::max (1, (int) std::ceil (sr / 50000.0));

    decim = std::max (1, (int) std::lround (sr / kTargetRate));
    decWindow = window / decim;
    const double srD = sr / decim;

    taps = decim > 1 ? kFirTaps : 1;
    fir.assign ((size_t) taps, 1.0f);
    if (decim > 1)
    {
        const double fc = 0.5 / decim;  // cycles per input sample: cutoff at the decimated Nyquist
        const int M = taps / 2;
        double sum = 0.0;
        for (int n = 0; n < taps; ++n)
        {
            const double k = n - M;
            const double x = 2.0 * fc * k;
            const double sinc = k == 0 ? 1.0 : std::sin (M_PI * x) / (M_PI * x);
            const double w = 0.42 - 0.5 * std::cos (2.0 * M_PI * n / (taps - 1))
                                  + 0.08 * std::cos (4.0 * M_PI * n / (taps - 1));
            fir[(size_t) n] = (float) (sinc * w);
            sum += fir[(size_t) n];
        }
        for (auto& c : fir) c = (float) (c / sum);
    }

    lagMinD = std::max (2, (int) std::floor (srD / (maxHz * 1.03)));
    lagMaxD = (int) std::ceil (srD / (minHz * 0.97)) + 1;
    lagMaxD = std::min (lagMaxD, decWindow - 4);
    lagMinD = std::min (lagMinD, lagMaxD - 1);

    ring.assign ((size_t) window, 0.0f);
    work.assign ((size_t) window, 0.0f);
    hist.assign ((size_t) taps * 2, 0.0f);
    dring.assign ((size_t) decWindow, 0.0f);
    dwork.assign ((size_t) decWindow, 0.0f);
    nsdf.assign ((size_t) lagMaxD + 4, 0.0f);
    refN.assign ((size_t) (2 * decim + 8), 0.0f);
    peakLag.assign ((size_t) lagMaxD / 2 + 4, 0.0f);
    peakVal.assign ((size_t) lagMaxD / 2 + 4, 0.0f);
    cum.assign ((size_t) window + 1, 0.0);
    reset();
}

void PitchDetector::reset()
{
    std::fill (ring.begin(), ring.end(), 0.0f);
    std::fill (hist.begin(), hist.end(), 0.0f);
    std::fill (dring.begin(), dring.end(), 0.0f);
    writePos = sinceHop = histPos = decCount = dpos = 0;
}

int PitchDetector::process (const float* mono, int numSamples, Estimate* out, int maxOut)
{
    if (ring.empty()) return 0;
    int written = 0;
    for (int i = 0; i < numSamples; ++i)
    {
        const float x = mono[i];
        ring[(size_t) writePos] = x;
        if (++writePos == window) writePos = 0;

        // Decimating FIR: only evaluate the filter on the samples we keep.
        hist[(size_t) histPos] = hist[(size_t) (histPos + taps)] = x;
        if (++histPos == taps) histPos = 0;
        if (++decCount == decim)
        {
            decCount = 0;
            dring[(size_t) dpos] = taps > 1 ? dotProduct (hist.data() + histPos, fir.data(), taps) : x;
            if (++dpos == decWindow) dpos = 0;
        }

        if (++sinceHop == hop)
        {
            sinceHop = 0;
            if (written < maxOut)
                computeEstimate (out[written++]);
        }
    }
    return written;
}

void PitchDetector::computeEstimate (Estimate& e)
{
    e = Estimate{};
    const int W = window;

    // Linearise the full-rate window, oldest first.
    std::memcpy (work.data(), ring.data() + writePos, (size_t) (W - writePos) * sizeof (float));
    std::memcpy (work.data() + (W - writePos), ring.data(), (size_t) writePos * sizeof (float));

    double sum = 0.0, sumSq = 0.0;
    for (int i = 0; i < W; ++i) { const double v = work[(size_t) i]; sum += v; sumSq += v * v; }
    const double rms = std::sqrt (sumSq / W);
    e.levelDb = rms > 1e-6 ? std::max (-120.0f, 20.0f * std::log10 ((float) rms)) : -120.0f;
    if (rms <= 1e-6) return;

    const float mean = (float) (sum / W);
    for (int i = 0; i < W; ++i) work[(size_t) i] -= mean;

    // Linearise the decimated window.
    const int N = decWindow;
    std::memcpy (dwork.data(), dring.data() + dpos, (size_t) (N - dpos) * sizeof (float));
    std::memcpy (dwork.data() + (N - dpos), dring.data(), (size_t) dpos * sizeof (float));
    double dsum = 0.0;
    for (int i = 0; i < N; ++i) dsum += dwork[(size_t) i];
    const float dmean = (float) (dsum / N);
    cum[0] = 0.0;
    for (int i = 0; i < N; ++i)
    {
        const float v = dwork[(size_t) i] - dmean;
        dwork[(size_t) i] = v;
        cum[(size_t) i + 1] = cum[(size_t) i] + (double) v * v;
    }
    if (cum[(size_t) N] < 1e-14) return;

    // NSDF on the decimated window over lags 1 .. lagMaxD + 1.
    const float* x = dwork.data();
    for (int lag = 1; lag <= lagMaxD + 1; ++lag)
    {
        const double m = cum[(size_t) (N - lag)] + (cum[(size_t) N] - cum[(size_t) lag]);
        nsdf[(size_t) lag] = m > 1e-14 ? (float) (2.0 * dotProduct (x, x + lag, N - lag) / m) : 0.0f;
    }

    // Key maxima: the highest point of each positive lobe after the first zero crossing,
    // stored with their interpolated height (coarse lag in decimated samples).
    int numPeaks = 0;
    {
        int i = 1;
        while (i <= lagMaxD + 1 && nsdf[(size_t) i] > 0.0f) ++i;      // skip the lobe around lag 0
        while (i <= lagMaxD + 1)
        {
            while (i <= lagMaxD + 1 && nsdf[(size_t) i] <= 0.0f) ++i;
            int best = -1;
            while (i <= lagMaxD + 1 && nsdf[(size_t) i] > 0.0f)
            {
                if (best < 0 || nsdf[(size_t) i] > nsdf[(size_t) best]) best = i;
                ++i;
            }
            if (best >= lagMinD && best <= lagMaxD
                && nsdf[(size_t) best] >= nsdf[(size_t) best - 1]
                && nsdf[(size_t) best] >= nsdf[(size_t) best + 1] && numPeaks < (int) peakLag.size())
            {
                float o = 0.0f, v = 0.0f;
                parabolic (nsdf[(size_t) best - 1], nsdf[(size_t) best], nsdf[(size_t) best + 1], o, v);
                peakLag[(size_t) numPeaks] = (float) best + o;
                peakVal[(size_t) numPeaks] = std::min (1.0f, v);
                ++numPeaks;
            }
        }
    }
    int highestIdx = -1;
    for (int k = 0; k < numPeaks; ++k)
        if (highestIdx < 0 || peakVal[(size_t) k] > peakVal[(size_t) highestIdx]) highestIdx = k;
    if (highestIdx < 0 || peakVal[(size_t) highestIdx] < kMinPeak) return;

    // Full-rate NSDF refinement of one coarse peak. Integer lags on the decimated grid can
    // straddle a narrow peak and under-read it, so candidates are compared on these values.
    bool fullReady = false;
    auto refine = [&] (float coarseLag, float coarseVal, double& lagOut, float& valOut)
    {
        lagOut = (double) coarseLag * decim;
        valOut = coarseVal;
        const int c = (int) std::lround (lagOut);
        const int lo = std::max (2, c - decim - 2);
        const int hi = std::min (W / 2, c + decim + 2);
        if (hi - lo < 2 || hi - lo >= (int) refN.size()) return;
        if (! fullReady)
        {
            cum[0] = 0.0;
            for (int i = 0; i < W; ++i) cum[(size_t) i + 1] = cum[(size_t) i] + (double) work[(size_t) i] * work[(size_t) i];
            fullReady = true;
        }
        const float* w = work.data();
        int bestK = -1;
        for (int t = lo; t <= hi; ++t)
        {
            const double m = cum[(size_t) (W - t)] + (cum[(size_t) W] - cum[(size_t) t]);
            const float v = m > 1e-14 ? (float) (2.0 * dotProduct (w, w + t, W - t) / m) : 0.0f;
            refN[(size_t) (t - lo)] = v;
            if (bestK < 0 || v > refN[(size_t) bestK]) bestK = t - lo;
        }
        if (bestK > 0 && bestK < hi - lo)
        {
            float o = 0.0f, pv = 0.0f;
            parabolic (refN[(size_t) bestK - 1], refN[(size_t) bestK], refN[(size_t) bestK + 1], o, pv);
            lagOut = (double) (lo + bestK) + o;
            valOut = std::min (1.0f, pv);
        }
    };

    double lagFull = 0.0, lagH = 0.0;
    float clarity = 0.0f, valH = 0.0f;
    refine (peakLag[(size_t) highestIdx], peakVal[(size_t) highestIdx], lagH, valH);
    lagFull = lagH; clarity = valH;

    // MPM selection: the first key maximum within kPeakThreshold of the highest one.
    // Only peaks that could plausibly pass (coarse height) are refined, a few at most.
    int refinements = 0;
    for (int k = 0; k < highestIdx && refinements < 8; ++k)
    {
        if (peakVal[(size_t) k] < 0.6f * peakVal[(size_t) highestIdx]) continue;
        double lagK; float valK;
        refine (peakLag[(size_t) k], peakVal[(size_t) k], lagK, valK);
        ++refinements;
        if (valK >= kPeakThreshold * valH) { lagFull = lagK; clarity = valK; break; }
    }

    const double hz = sr / lagFull;
    if (! (hz >= minHz * 0.97 && hz <= maxHz * 1.03)) return;

    e.valid = true;
    e.hz = (float) hz;
    e.clarity = std::max (0.0f, std::min (1.0f, clarity));
}

// ───────────────────────────── NoteTracker ─────────────────────────────

namespace
{
constexpr float kStartClarity = 0.8f;
constexpr float kKeepClarity  = 0.5f;
constexpr float kGateHyst     = 3.0f;
constexpr float kAgree        = 0.35f;   // semitones (35 cents)
constexpr float kStayInside   = 0.65f;   // held note is kept while its tracked pitch is within 65 cents of centre
constexpr float kFollow       = 0.70f;   // estimates this close to the tracked pitch are the same note (vibrato, bends)
constexpr int   kStableHops   = 3;

inline float hzToMidi (float hz) { return 69.0f + 12.0f * std::log2 (hz / 440.0f); }
inline float midiToHz (float m)  { return 440.0f * std::exp2 ((m - 69.0f) / 12.0f); }
} // namespace

void NoteTracker::prepare (double hopRateHz)
{
    hopRate = hopRateHz > 0.0 ? hopRateHz : 187.5;
    // Time constant about 270 ms for the anchor that follows slow drift and vibrato.
    anchorCoef = (float) (1.0 - std::exp (-1.0 / (hopRate * 0.27)));
    reset();
}

void NoteTracker::reset()
{
    held = false; note = -1; cents = 0.0f;
    candidateNote = -1; candidateCount = lowCount = 0;
    candLast = candSum = anchor = 0.0f;
}

void NoteTracker::setGateDb (float db) { gateDb = db; }

int NoteTracker::process (const Estimate& e, NoteEvent* out)
{
    int n = 0;
    auto makeEvent = [] (NoteEvent::Type t, int midi, float m) {
        NoteEvent ev;
        ev.type = t;
        ev.midiNote = midi;
        ev.hz = midiToHz (m);
        ev.cents = std::max (-50.0f, std::min (50.0f, (m - (float) midi) * 100.0f));
        return ev;
    };
    auto stopNote = [&] {
        out[n++] = makeEvent (NoteEvent::Type::Off, note, anchor);
        held = false; note = -1; cents = 0.0f;
        lowCount = 0;
    };
    auto followAnchor = [&] (float m) {
        anchor += anchorCoef * (m - anchor);
        cents = std::max (-50.0f, std::min (50.0f, (anchor - (float) note) * 100.0f));
    };
    auto clearCandidate = [&] { candidateCount = 0; candidateNote = -1; candSum = 0.0f; };

    const bool levelOk = e.levelDb > (held ? gateDb - kGateHyst : gateDb);
    const bool detected = e.valid && e.hz > 0.0f;

    if (held && ! levelOk)
    {
        stopNote();
        clearCandidate();
        return n;
    }

    if (held)
    {
        if (! detected || e.clarity < kKeepClarity)
        {
            clearCandidate();
            if (++lowCount >= kStableHops) stopNote();
            return n;
        }
        lowCount = 0;
    }

    const bool strong = detected && e.clarity > kStartClarity && levelOk;
    if (! strong)
    {
        // A mid-clarity reading still keeps the held note alive and follows its pitch.
        if (held && detected)
        {
            const float m = hzToMidi (e.hz);
            if (std::fabs (m - anchor) < kFollow && std::fabs (anchor - (float) note) < kStayInside)
                followAnchor (m);
        }
        clearCandidate();
        return n;
    }

    const float m = hzToMidi (e.hz);

    if (held && std::fabs (m - anchor) < kFollow)
    {
        followAnchor (m);
        if (std::fabs (anchor - (float) note) < kStayInside) return n;
        // The tracked pitch has drifted past the next semitone's border: move there.
        const int newNote = (int) std::lround (anchor);
        stopNote();
        clearCandidate();
        held = true; note = newNote; lowCount = 0;
        cents = std::max (-50.0f, std::min (50.0f, (anchor - (float) note) * 100.0f));
        out[n++] = makeEvent (NoteEvent::Type::On, note, anchor);
        return n;
    }

    // Candidate: consecutive estimates agreeing with each other.
    if (candidateCount > 0 && std::fabs (m - candLast) < kAgree)
    {
        ++candidateCount;
        candSum += m;
    }
    else
    {
        candidateCount = 1;
        candSum = m;
    }
    candLast = m;

    if (candidateCount >= kStableHops)
    {
        const float mean = candSum / (float) candidateCount;
        const int newNote = (int) std::lround (mean);
        clearCandidate();
        if (held && newNote == note)
        {
            anchor = mean;                    // same semitone, just a new reference pitch
            cents = std::max (-50.0f, std::min (50.0f, (anchor - (float) note) * 100.0f));
            return n;
        }
        if (held) stopNote();
        held = true;
        note = newNote;
        anchor = mean;
        lowCount = 0;
        cents = std::max (-50.0f, std::min (50.0f, (mean - (float) note) * 100.0f));
        out[n++] = makeEvent (NoteEvent::Type::On, note, mean);
    }
    return n;
}
} // namespace spn
