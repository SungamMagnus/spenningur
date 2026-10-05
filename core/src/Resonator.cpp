// Resonator bank, envelope and limiter. See Resonator.h for the contract.
//
// Voice: y[n] = x[n] + g * DC( LP( lagrange3( y, d ) ) )
//   - d is chosen so that the whole loop (delay + interpolator + low-pass + DC blocker) has a period of
//     sr / f0, i.e. the filters' phase delay at f0 is subtracted from the delay.
//   - g is chosen so that |loop| at f0 gives T60 = ringSec, clamped below 0.9995.
//   - the excitation is scaled by sqrt(1 - g) so white noise comes out at ~0.7 of its input rms at any
//     pitch / RING setting, and the bank stays bounded for full-scale input.
// All control work happens on a fixed 16-sample grid so the output does not depend on the host block size.
#include "spenningur/Resonator.h"

#include <algorithm>
#include <cmath>
#include <complex>

namespace spn
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr float kMaxLoopGain = 0.9995f;
constexpr float kMinHz = 30.0f, kMaxHz = 2500.0f;
constexpr float kInternalExcitation = 0.06f;    // rms of the internal noise at env = 1, before the per-voice sqrt(1-g) scale
constexpr float kNoiseLpA = 0.8f;               // the internal noise is one-pole low-passed (about 1.8 kHz at 48 kHz): soft, not hissy

struct LoopResponse { double mag, excess; };

// Response of Lagrange(t) * one-pole LP * DC blocker, relative to the integer delay.
// excess = phase delay of the whole thing minus the nominal fractional delay t (samples).
LoopResponse loopResponse (double w, double t, double a, double R)
{
    using C = std::complex<double>;
    const C z1 = std::polar (1.0, -w);
    const double c0 = -t * (t - 1.0) * (t - 2.0) / 6.0;
    const double c1 = (t + 1.0) * (t - 1.0) * (t - 2.0) / 2.0;
    const double c2 = -(t + 1.0) * t * (t - 2.0) / 2.0;
    const double c3 = (t + 1.0) * t * (t - 1.0) / 6.0;
    const C L = c0 * std::polar (1.0, w) + c1 + c2 * z1 + c3 * z1 * z1;
    const C LP = (1.0 - a) / (1.0 - a * z1);
    const C DC = (1.0 - z1) / (1.0 - R * z1);
    const C H = L * LP * DC;
    return { std::abs (H), -std::arg (H) / w - t };
}

inline float lagrange (const float* buf, int mask, int w, float dd)
{
    const int i = (int) dd;
    const float t = dd - (float) i;
    const float c0 = -t * (t - 1.0f) * (t - 2.0f) * (1.0f / 6.0f);
    const float c1 = (t + 1.0f) * (t - 1.0f) * (t - 2.0f) * 0.5f;
    const float c2 = -(t + 1.0f) * t * (t - 2.0f) * 0.5f;
    const float c3 = (t + 1.0f) * t * (t - 1.0f) * (1.0f / 6.0f);
    const int p = w - i;
    return c0 * buf[(p + 1) & mask] + c1 * buf[p & mask] + c2 * buf[(p - 1) & mask] + c3 * buf[(p - 2) & mask];
}

inline float sanitise (float x) { return (x > -1.0e6f && x < 1.0e6f) ? x : 0.0f; }
} // namespace

// ---------------------------------------------------------------------------------------------
// Resonator
// ---------------------------------------------------------------------------------------------
void Resonator::prepare (double sampleRate, int /*maxBlockSize*/)
{
    sr = sampleRate > 1000.0 ? sampleRate : 48000.0;
    int need = (int) std::ceil (sr / (double) kMinHz * 1.3) + 16;
    bufSize = 64;
    while (bufSize < need) bufSize <<= 1;
    for (auto& v : voices)
    {
        v.buf.assign ((size_t) bufSize, 0.0f);
        v.mask = bufSize - 1;
    }
    hpA = (float) std::exp (-2.0 * kPi * 80.0 / sr);
    dcR = (float) std::exp (-2.0 * kPi * 5.0 / sr);
    fadeStep = (float) (1.0 / (0.020 * sr));
    panCoef = (float) (1.0 - std::exp (-1.0 / (0.010 * sr)));
    bankCoef = (float) (1.0 - std::exp (-1.0 / (0.020 * sr)));
    susCoef = (float) (1.0 - std::exp (-1.0 / (0.010 * sr)));
    setFilterCoeffs();
    setParams (params);
    reset();
}

void Resonator::clearVoice (Voice& v)
{
    std::fill (v.buf.begin(), v.buf.end(), 0.0f);
    v.w = 0;
    v.lpZ = v.dcX = v.dcY = 0.0f;
    v.xf = 1.0f;
}

void Resonator::reset()
{
    for (auto& v : voices)
    {
        clearVoice (v);
        v.active = false;
        v.lv = 0.0f;
    }
    numNotes = 0;
    chunkPhase = 0;
    hpX = hpY = 0.0f;
    stage = idle;
    env = gate = envLevel = 0.0f;
    bankGain = bankTarget = 1.0f;
    noiseState = 22222u;
    noiseLp = 0.0f;
}

void Resonator::setFilterCoeffs()
{
    const double hz = std::min ((double) params.dampHz, sr * 0.45);
    lpA = (float) std::exp (-2.0 * kPi * std::max (hz, 200.0) / sr);
}

void Resonator::setParams (const ResonatorParams& pIn)
{
    ResonatorParams p = pIn;
    p.ringSec   = std::clamp (p.ringSec, 0.2f, 8.0f);
    p.dampHz    = std::clamp (p.dampHz, 1000.0f, 12000.0f);
    p.spreadOct = std::clamp (p.spreadOct, 0.0f, 3.0f);
    p.glideMs   = std::clamp (p.glideMs, 0.0f, 200.0f);
    p.attackMs  = std::clamp (p.attackMs, 1.0f, 2000.0f);
    p.decayMs   = std::clamp (p.decayMs, 10.0f, 2000.0f);
    p.sustain   = std::clamp (p.sustain, 0.0f, 1.0f);
    p.releaseMs = std::clamp (p.releaseMs, 20.0f, 8000.0f);

    const bool spreadChanged = p.spreadOct != params.spreadOct;
    const bool dampChanged = p.dampHz != params.dampHz;
    params = p;

    attInc = (float) (1.0 / std::max (1.0, params.attackMs * 0.001 * sr));
    decCoef = (float) std::exp (-4.6 / std::max (1.0, params.decayMs * 0.001 * sr));
    relCoef = (float) std::exp (-6.907755 / std::max (1.0, params.releaseMs * 0.001 * sr));

    if (dampChanged) setFilterCoeffs();
    if (numNotes > 0 && (dampChanged || spreadChanged))
    {
        updateTargets (false);
        for (int i = 0; i < numNotes; ++i)
            if (voices[i].active) tuneVoice (voices[i], voiceFreq (i), false, 2);
    }
}

float Resonator::voiceFreq (int i) const
{
    const int n = numNotes;
    const int m = (i + 1) / 2;
    const int maxm = std::max (1, n / 2);
    const int shift = (int) std::lround ((double) params.spreadOct * m / maxm);
    const int oct = (i % 2 == 1) ? shift : -shift;
    double f = 440.0 * std::pow (2.0, (notes[i] - 69) / 12.0 + oct);
    while (f < kMinHz) f *= 2.0;
    while (f > kMaxHz) f *= 0.5;
    return (float) f;
}

void Resonator::tuneVoice (Voice& v, float freq, bool snap, int mode)   // 0 swap+crossfade, 1 slew(glideMs), 2 slew(>=20 ms)
{
    freq = std::min (freq, (float) (sr * 0.2));
    v.freq = freq;
    const double w = 2.0 * kPi * (double) freq / sr;
    const double P = sr / (double) freq;
    double dd = P, excess = 0.0, mag = 1.0;
    for (int k = 0; k < 4; ++k)
    {
        const double i = std::floor (dd);
        const auto r = loopResponse (w, dd - i, (double) lpA, (double) dcR);
        excess = r.excess;
        mag = r.mag;
        dd = P - excess;
    }
    dd = std::clamp (dd, 2.0, (double) (bufSize - 6));
    v.excess = (float) excess;
    v.mag = (float) std::max (mag, 0.05);
    v.dTarget = (float) dd;

    if (snap)
    {
        v.d = v.dTarget;
        v.dStep = 0.0f;
        v.xf = 1.0f;
        return;
    }
    if (mode == 0)
    {
        if (std::fabs (v.dTarget - v.d) < 1.0e-4f) return;
        v.dOld = v.d;
        v.d = v.dTarget;
        v.dStep = 0.0f;
        v.xf = 0.0f;
        v.xfInc = fadeStep;
    }
    else
    {
        const double ms = mode == 2 ? std::max ((double) params.glideMs, 20.0)   // parameter-driven retunes always slew
                                    : (double) params.glideMs;
        const double n = std::max (1.0, ms * 0.001 * sr);
        v.dStep = (v.dTarget - v.d) / (float) n;
    }
}

void Resonator::updateTargets (bool snap)
{
    // pans: by voice order across the field, width follows SPREAD
    const float width = std::min (1.0f, params.spreadOct / 1.5f);
    for (int i = 0; i < kMaxVoices; ++i)
    {
        Voice& v = voices[i];
        if (i >= numNotes) continue;
        const float pos = numNotes > 1 ? -1.0f + 2.0f * (float) i / (float) (numNotes - 1) : 0.0f;
        const float pan = 0.5f + 0.5f * width * pos;
        v.panLT = (float) std::cos (pan * kPi * 0.5);
        v.panRT = (float) std::sin (pan * kPi * 0.5);
        if (snap) { v.panL = v.panLT; v.panR = v.panRT; }
    }
}

void Resonator::setChord (const int* midiNotes, int count)
{
    count = std::clamp (count, 0, kMaxVoices);
    if (midiNotes == nullptr) count = 0;
    numNotes = count;
    for (int i = 0; i < count; ++i) notes[i] = midiNotes[i];
    bankTarget = count > 0 ? 1.0f / std::sqrt ((float) count) : 1.0f;

    const int mode = params.glideMs > 0.0f ? 1 : 0;
    for (int i = 0; i < kMaxVoices; ++i)
    {
        Voice& v = voices[i];
        if (i < count)
        {
            const float f = voiceFreq (i);
            if (! v.active && v.lv <= 0.0f)
            {
                clearVoice (v);
                tuneVoice (v, f, true, 0);
                v.active = true;
            }
            else
            {
                v.active = true;
                tuneVoice (v, f, false, mode);
            }
        }
        else
            v.active = false;       // fades out and keeps ringing while it fades
    }
    updateTargets (false);
    for (int i = 0; i < count; ++i)
        if (voices[i].lv <= 0.0f && voices[i].xf >= 1.0f && voices[i].d == voices[i].dTarget && voices[i].dStep == 0.0f)
        {
            voices[i].panL = voices[i].panLT;       // a voice that is just starting is placed immediately
            voices[i].panR = voices[i].panRT;
        }
}

void Resonator::noteOn()
{
    stage = attack;
}

void Resonator::noteOff()
{
    if (stage != idle) stage = release;
}

inline void Resonator::stepEnvelope()
{
    switch (stage)
    {
        case idle:
            break;
        case attack:
            env += attInc;
            gate = std::min (1.0f, gate + attInc);
            if (env >= 1.0f) { env = 1.0f; gate = 1.0f; stage = decay; }
            break;
        case decay:
        {
            const float s = params.sustain;
            env = s + (env - s) * decCoef;
            if (env - s < 1.0e-4f) { env = s; stage = sustainStage; }
            gate = env;                                  // the full ADSR is the output VCA
            break;
        }
        case sustainStage:
            env += (params.sustain - env) * susCoef;
            gate = env;
            break;
        case release:
            env *= relCoef;
            gate *= relCoef;
            if (gate < 1.0e-4f) { gate = 0.0f; env = 0.0f; stage = idle; }
            break;
    }
    envLevel = env;
}

void Resonator::updateGains()
{
    const double k = -6.907755278982137 / ((double) params.ringSec * sr);
    for (auto& v : voices)
    {
        if (! v.active && v.lv <= 0.0f) continue;
        const double P = (double) v.d + (double) v.excess;
        float g = (float) (std::exp (k * P) / (double) v.mag);
        g = std::min (std::max (g, 0.0f), kMaxLoopGain);
        v.g = g;
        v.inScale = std::sqrt (std::max (0.0f, 1.0f - g));
    }
}

void Resonator::process (const float* in, float* outL, float* outR, int n)
{
    int pos = 0;
    while (pos < n)
    {
        if (chunkPhase == 0) updateGains();
        const int len = std::min (n - pos, kChunk - chunkPhase);

        for (int s = 0; s < len; ++s)
        {
            const float xin = sanitise (in[pos + s]);

            // input high-pass (80 Hz) and internal excitation
            const float hp = hpA * (hpY + xin - hpX);
            hpX = xin;
            hpY = hp;

            noiseState ^= noiseState << 13;
            noiseState ^= noiseState >> 17;
            noiseState ^= noiseState << 5;
            const float white = ((float) (int) noiseState) * (1.7320508f / 2147483648.0f);   // unit rms
            noiseLp = kNoiseLpA * noiseLp + (1.0f - kNoiseLpA) * white;
            const float noise = noiseLp * 3.0f;                                               // 3 = sqrt((1+a)/(1-a)): unit rms again

            stepEnvelope();
            denorm = -denorm;
            const float exc = hp + noise * env * kInternalExcitation + denorm;

            bankGain += (bankTarget - bankGain) * bankCoef;

            float mixL = 0.0f, mixR = 0.0f;
            for (auto& v : voices)
            {
                if (! v.active && v.lv <= 0.0f) continue;

                // fades and pans
                if (v.active) { v.lv = std::min (1.0f, v.lv + fadeStep); }
                else
                {
                    v.lv -= fadeStep;
                    if (v.lv <= 0.0f) { v.lv = 0.0f; clearVoice (v); continue; }
                }
                v.panL += (v.panLT - v.panL) * panCoef;
                v.panR += (v.panRT - v.panR) * panCoef;

                // glide
                if (v.dStep != 0.0f)
                {
                    v.d += v.dStep;
                    if ((v.dStep > 0.0f && v.d >= v.dTarget) || (v.dStep < 0.0f && v.d <= v.dTarget))
                    {
                        v.d = v.dTarget;
                        v.dStep = 0.0f;
                    }
                }

                float fb = lagrange (v.buf.data(), v.mask, v.w, v.d);
                if (v.xf < 1.0f)
                {
                    const float fo = lagrange (v.buf.data(), v.mask, v.w, v.dOld);
                    fb = fo + v.xf * (fb - fo);
                    v.xf = std::min (1.0f, v.xf + v.xfInc);
                }

                v.lpZ = (1.0f - lpA) * fb + lpA * v.lpZ;
                const float dcy = v.lpZ - v.dcX + dcR * v.dcY;
                v.dcX = v.lpZ;
                v.dcY = dcy;

                float y = exc * v.inScale + v.g * dcy;
                y = std::max (-100.0f, std::min (100.0f, y));
                v.buf[(size_t) (v.w & v.mask)] = y;
                v.w = (v.w + 1) & v.mask;

                const float o = y * v.lv;
                mixL += o * v.panL;
                mixR += o * v.panR;
            }

            const float gn = bankGain * gate;
            outL[pos + s] = mixL * gn;
            outR[pos + s] = mixR * gn;
        }
        pos += len;
        chunkPhase = (chunkPhase + len) % kChunk;
    }
}

// ---------------------------------------------------------------------------------------------
// Limiter
// ---------------------------------------------------------------------------------------------
namespace
{
constexpr float kCeilingDb = -1.0f;
constexpr float kKneeDb = 6.0f;

// Static curve: gain reduction (dB, >= 0) for an instantaneous peak level in dB. Quadratic soft knee that
// meets a hard ceiling (ratio infinity) at kCeilingDb + knee/2, so the output never exceeds the ceiling.
inline float reductionDb (float xDb)
{
    const float T = kCeilingDb - 0.5f * kKneeDb;
    if (xDb <= T) return 0.0f;
    if (xDb >= T + kKneeDb) return xDb - kCeilingDb;
    const float u = xDb - T;
    return u * u / (2.0f * kKneeDb);
}
} // namespace

void Limiter::prepare (double sampleRate)
{
    sr = sampleRate > 1000.0 ? sampleRate : 48000.0;
    reset();
}

void Limiter::reset()
{
    gain = 1.0f;
    det = 0.0f;
    grDb = 0.0f;
}

void Limiter::process (float* l, float* r, int numSamples)
{
    if (! enabled)
    {
        gain = 1.0f;
        det = 0.0f;
        grDb = 0.0f;
        return;
    }
    const float aAtt = (float) std::exp (-1.0 / (0.001 * sr));
    const float aRel = (float) std::exp (-1.0 / (0.100 * sr));
    const float ceiling = std::pow (10.0f, kCeilingDb / 20.0f);
    const float aDet = (float) std::exp (-1.0 / (0.010 * sr));
    float worst = 1.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        float a = l[i], b = r[i];
        if (! (a > -1.0e6f && a < 1.0e6f)) a = 0.0f;
        if (! (b > -1.0e6f && b < 1.0e6f)) b = 0.0f;
        const float inst = std::max (std::fabs (a), std::fabs (b));
        det = std::max (inst, det * aDet);          // peak detector: instant attack, 10 ms release
        const float peak = det;

        float target = 1.0f;
        if (peak > 1.0e-6f)
        {
            const float xDb = 20.0f * std::log10 (peak);
            const float red = reductionDb (xDb);
            if (red > 0.0f) target = std::pow (10.0f, -red / 20.0f);
        }
        const float coef = target < gain ? aAtt : aRel;
        gain = target + (gain - target) * coef;
        if (gain > 0.99999f) gain = 1.0f;
        worst = std::min (worst, gain);

        // final safety: smooth saturation of anything that still pokes through while the gain catches up
        auto sat = [ceiling] (float x)
        {
            const float start = ceiling + 0.04f, top = 0.99f;
            const float ax = std::fabs (x);
            if (ax <= start) return x;
            const float y = start + (top - start) * std::tanh ((ax - start) / (top - start));
            return x < 0.0f ? -y : y;
        };
        l[i] = sat (a * gain);
        r[i] = sat (b * gain);
    }
    grDb = worst < 0.9995f ? -20.0f * std::log10 (worst) : 0.0f;
}

} // namespace spn
