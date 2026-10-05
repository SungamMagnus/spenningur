#pragma once

#include <vector>

/* The resonator bank, its envelope and the output limiter. No JUCE, no
 * allocation after prepare(). Public API is a contract shared with the
 * plug-in; the private sections belong to the implementation. */
namespace spn
{

struct ResonatorParams
{
    float ringSec   = 4.5f;      // T60 of each voice, 0.2 .. 8
    float dampHz    = 6500.0f;   // low-pass in the loop, 1000 .. 12000
    float spreadOct = 1.2f;      // 0 .. 3: voices spread across this many octaves, and across the stereo field
    float glideMs   = 40.0f;     // retune slew on chord change, 0 .. 200 (0 = swap with a 20 ms crossfade)
    float attackMs  = 30.0f;     // 1 .. 2000
    float decayMs   = 413.0f;    // 10 .. 2000
    float sustain   = 0.6f;      // 0 .. 1
    float releaseMs = 2000.0f;   // 20 .. 8000
};

/** A bank of tuned feedback combs excited by the input, shaped by an ADSR.
 *
 *  Each voice: fractional-delay feedback comb (Lagrange 3rd order), a one-pole
 *  low-pass in the loop (dampHz), a DC blocker; loop gain derived per voice from
 *  its delay length so every voice has the same T60 (ringSec), clamped below
 *  0.9995. The bank is scaled by 1/sqrt(voices).
 *
 *  The ADSR is retriggered by noteOn() and released by noteOff(). It scales the
 *  bank output AND drives a small internal excitation (a soft noise burst shaped
 *  by the same envelope) into the bank, so the SUSTAIN stage keeps the chord
 *  sounding while a note is held even when the input has decayed - that is what
 *  lets a plucked input become a pad. At sustain 0 the chord is a pluck that
 *  dies on ringSec. */
class Resonator
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    void setParams (const ResonatorParams& p);

    /** Retune to these MIDI notes (1..6). Does not retrigger the envelope. */
    void setChord (const int* midiNotes, int count);

    void noteOn();      // retrigger the ADSR (once per chord change)
    void noteOff();     // gate closed: ADSR to release

    /** `in` is the excitation (already scaled by the IN gain). Writes the wet
     *  signal, stereo. May be called with in == outL aliasing only if numSamples
     *  fits maxBlockSize; the implementation must tolerate it. */
    void process (const float* in, float* outL, float* outR, int numSamples);

    float envelopeLevel() const { return envLevel; }    // 0..1, for the UI

private:
    static constexpr int kMaxVoices = 6;
    static constexpr int kChunk = 16;               // control-rate step (samples), independent of the host block size

    struct Voice                                    // implementation detail; free to change
    {
        std::vector<float> buf;                     // comb state y[n], power-of-two ring
        int mask = 0, w = 0;
        float d = 100.0f, dTarget = 100.0f, dStep = 0.0f;   // loop delay (samples), slewed to dTarget
        float dOld = 100.0f, xf = 1.0f, xfInc = 0.0f;       // hard-swap crossfade (second read tap)
        float excess = 0.0f, mag = 1.0f;            // filter phase delay beyond the nominal and magnitude at f0
        float g = 0.9f, inScale = 0.2f;             // loop gain, excitation scale
        float lpZ = 0.0f, dcX = 0.0f, dcY = 0.0f;
        float lv = 0.0f;                            // fade level 0..1
        float panL = 0.7071f, panR = 0.7071f, panLT = 0.7071f, panRT = 0.7071f;
        float freq = 220.0f;
        bool active = false;
    };

    void clearVoice (Voice& v);
    float voiceFreq (int i) const;
    void tuneVoice (Voice& v, float freq, bool snap, int mode);   // mode 0 hard swap/crossfade, 1 slew over glide
    void updateTargets (bool snap);
    void updateGains();
    void setFilterCoeffs();
    void stepEnvelope();

    enum Stage { idle, attack, decay, sustainStage, release };

    double sr = 48000.0;
    float envLevel = 0.0f;
    ResonatorParams params;
    Voice voices[kMaxVoices];
    int notes[kMaxVoices] = { 60, 64, 67, 71, 74, 77 };
    int numNotes = 0;
    int chunkPhase = 0;
    int bufSize = 0;
    // shared filter coefficients
    float lpA = 0.5f, dcR = 0.999f, hpA = 0.99f, hpX = 0.0f, hpY = 0.0f;
    float fadeStep = 0.01f, panCoef = 0.01f, bankGain = 1.0f, bankTarget = 1.0f, bankCoef = 0.001f;
    // envelope
    Stage stage = idle;
    float env = 0.0f, gate = 0.0f;                  // env: ADSR (excitation, UI); gate: output VCA (attack/release only)
    float attInc = 0.01f, decCoef = 0.999f, relCoef = 0.9999f, susCoef = 0.001f;
    unsigned noiseState = 22222u;
    float noiseLp = 0.0f;
    float denorm = 1e-18f;
};

/** Soft-knee peak limiter for the wet path. Ceiling about -1 dBFS. */
class Limiter
{
public:
    void prepare (double sampleRate);
    void reset();
    void setEnabled (bool on) { enabled = on; }
    void process (float* l, float* r, int numSamples);
    float gainReductionDb() const { return grDb; }      // 0 when idle, positive when reducing

private:
    double sr = 48000.0;
    bool enabled = true;
    float gain = 1.0f, grDb = 0.0f, det = 0.0f;
};

} // namespace spn
