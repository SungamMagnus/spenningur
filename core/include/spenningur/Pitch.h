#pragma once

#include <vector>

/* Monophonic pitch detection and note tracking. No JUCE, no allocation after
 * prepare(). Public API is a contract shared with the plug-in; the private
 * sections belong to the implementation. */
namespace spn
{

/** How wide a band the detector searches. Trades reach for latency. */
enum class Range { Small = 0, Medium = 1, Large = 2 };

/*  Range    searches about   window  hop
 *  Small    C3 to C5          1024    256     (130.8 .. 523.3 Hz)
 *  Medium   E2 to E6          2048    256     ( 82.4 .. 1318.5 Hz)
 *  Large    B0 to C7          4096    256     ( 30.9 .. 2093.0 Hz)
 */

/** One raw pitch reading, produced once per hop. */
struct Estimate
{
    bool  valid   = false;    // true when a periodicity peak was found inside the band
    float hz      = 0.0f;     // 0 when not valid
    float clarity = 0.0f;     // 0..1, normalised peak height (MPM NSDF peak)
    float levelDb = -120.0f;  // RMS of the analysis window, dBFS
};

/** McLeod Pitch Method over a sliding window. */
class PitchDetector
{
public:
    void prepare (double sampleRate, Range range);
    void reset();

    /** Feed mono samples. Writes one Estimate per completed hop into `out`
     *  (up to maxOut) and returns how many were written. Real-time safe. */
    int process (const float* mono, int numSamples, Estimate* out, int maxOut);

    int    windowSamples() const { return window; }
    int    hopSamples()    const { return hop; }
    double hopRateHz()     const { return hop > 0 ? sr / hop : 0.0; }

private:
    void computeEstimate (Estimate& e);

    double sr = 48000.0;
    int window = 2048, hop = 256;
    float minHz = 82.4f, maxHz = 1318.5f;

    // Full-rate sliding ring and its linearised copy (used for level and lag refinement).
    std::vector<float> ring, work;
    int writePos = 0, sinceHop = 0;

    // Decimated path (about 12 kHz) used for the coarse NSDF search.
    int decim = 4, taps = 1, decWindow = 512, lagMinD = 2, lagMaxD = 100;
    std::vector<float> fir, hist, dring, dwork, nsdf, refN, peakLag, peakVal;
    std::vector<double> cum;
    int histPos = 0, decCount = 0, dpos = 0;
};

/** A stable note starting or ending. */
struct NoteEvent
{
    enum class Type { On, Off };
    Type  type     = Type::On;
    int   midiNote = 0;       // nearest semitone, A4 = 69
    float hz       = 0.0f;
    float cents    = 0.0f;    // deviation from midiNote, -50..+50
};

/** Turns a stream of raw estimates into note events.
 *
 *  A note starts when estimates agree within 35 cents for 3 consecutive hops,
 *  clarity is above 0.8 and the level is above the gate. It ends when the level
 *  falls 3 dB below the gate (hysteresis) or clarity stays under 0.5 for 3 hops,
 *  or it is replaced: a held note that moves to a different stable semitone emits
 *  Off then On in the same call. A glide inside 35 cents of the held note does
 *  not retrigger. */
class NoteTracker
{
public:
    void prepare (double hopRateHz);
    void reset();
    void setGateDb (float db);

    /** Feed one estimate per hop. Writes up to 2 events into `out` (capacity 2)
     *  and returns how many. */
    int process (const Estimate& e, NoteEvent* out);

    bool isHeld() const { return held; }
    int  currentNote() const { return note; }
    float currentCents() const { return cents; }

private:
    double hopRate = 187.5;
    float gateDb = -58.0f;
    bool held = false;
    int note = -1;
    float cents = 0.0f;
    int candidateNote = -1, candidateCount = 0, lowCount = 0;
    float anchor = 0.0f;            // slow-tracked pitch of the held note, fractional MIDI
    float candLast = 0.0f, candSum = 0.0f;   // running state of the candidate streak
    float anchorCoef = 0.02f;
};

} // namespace spn
