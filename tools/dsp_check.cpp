// Dev tool: run the real SpenningurProcessor over synthetic monophonic melodies and
// report what comes out. Catches NaNs, runaway output, a mix control that is not
// transparent, and (once the core lands) a detector or chord engine that does nothing.
//
//   dsp_check            hard checks only; detector-dependent checks print a NOTE
//   dsp_check --strict   detector-dependent checks become failures too
//
// Hard failures (always): NaN/inf, output peak above ~1.5 with LIM on, MIX 0 not
// transparent, MIX 1 leaking dry, crashes on odd blocks. Exit code is non-zero if any fail.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <set>
#include <string>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "ParamIDs.h"
#include "PluginProcessor.h"
#include "spenningur/Harmony.h"

namespace
{
constexpr double kSr = 48000.0;
constexpr double kPi2 = 6.283185307179586;

bool gStrict = false;
int  gHardFails = 0;
int  gSoftFails = 0;

// ---------------------------------------------------------------- checks

void hardCheck (bool ok, const std::string& what)
{
    if (! ok)
    {
        ++gHardFails;
        std::printf ("    FAIL: %s\n", what.c_str());
    }
}

/** A check that depends on the detector / chord engine / resonator being real. */
void softCheck (bool ok, const std::string& what)
{
    if (ok) return;
    if (gStrict)
    {
        ++gHardFails;
        std::printf ("    FAIL (strict): %s\n", what.c_str());
    }
    else
    {
        ++gSoftFails;
        std::printf ("    NOTE: %s  [skipped without --strict; core may still be stubbed]\n", what.c_str());
    }
}

// ---------------------------------------------------------------- signals

enum class Sig { Sine, Saw, Vibrato, Hot, Silence };

const int kLine[8] = { 57, 60, 64, 62, 59, 65, 64, 57 };      // A3 C4 E4 D4 B3 F4 E4 A3
constexpr double kLead = 0.25, kSlot = 0.5, kGap = 0.08, kTail = 1.5;

double midiHz (double m) { return 440.0 * std::pow (2.0, (m - 69.0) / 12.0); }

std::string noteName (int m)
{
    return std::string (spn::rootName (((m % 12) + 12) % 12)) + std::to_string (m / 12 - 1);
}

std::vector<float> makeMelody (Sig sig)
{
    const int total = (int) std::lround ((kLead + 8 * kSlot + kTail) * kSr);
    std::vector<float> x ((size_t) total, 0.0f);
    if (sig == Sig::Silence)
        return x;

    const float amp = sig == Sig::Hot ? 1.0f : 0.3f;
    for (int n = 0; n < 8; ++n)
    {
        const int start = (int) std::lround ((kLead + n * kSlot) * kSr);
        const int len   = (int) std::lround ((kSlot - kGap) * kSr);
        const double f0 = midiHz (kLine[n]);
        double ph = 0.0;
        for (int i = 0; i < len && start + i < total; ++i)
        {
            const double t = (double) i / kSr;
            double f = f0;
            if (sig == Sig::Vibrato)
                f *= std::pow (2.0, 0.15 * std::sin (kPi2 * 5.5 * t) / 12.0);     // +-15 cents at 5.5 Hz
            ph += kPi2 * f / kSr;
            if (ph > kPi2) ph -= kPi2;

            double v;
            if (sig == Sig::Saw || sig == Sig::Hot)
            {
                v = 0.0;                                                          // band-limited saw
                for (int k = 1; k * f0 < 6000.0; ++k)
                    v += std::sin (k * ph) / k;
                v *= 0.6;
            }
            else
            {
                v = std::sin (ph) + (sig == Sig::Vibrato ? 0.25 * std::sin (2.0 * ph) : 0.0);
            }

            const double a = std::min (1.0, i / (0.005 * kSr));                    // 5 ms attack
            const double r = std::min (1.0, (len - i) / (0.010 * kSr));            // 10 ms release
            x[(size_t) (start + i)] = (float) (amp * v * a * r);
        }
    }
    return x;
}

// ---------------------------------------------------------------- running

struct Setup
{
    std::string name;
    Sig  sig = Sig::Sine;
    bool monoLayout = false;
    float rScale = 1.0f;                                  // right input = left * rScale (stereo layouts)
    std::function<void (SpenningurProcessor&)> params;    // applied before prepareToPlay
    std::vector<int> blocks { 64, 512 };
};

struct ChordSeen
{
    int rootPc, style, effRoot;
    bool inKey;
};

struct Result
{
    int block = 0;
    std::vector<int> notes;
    std::vector<ChordSeen> chords;
    int numChords = 0;
    float rms = 0.0f, peak = 0.0f, tailRms = 0.0f, bodyRms = 0.0f;
    float maxGr = 0.0f, maxEnv = 0.0f, maxDiff = 0.0f;
    bool finite = true;
    bool gateSeen = false;
    int  finalRoot = -1;
    double cpuPct = 0.0;
    std::string chordText, noteText;
};

void set (SpenningurProcessor& p, const char* id, float value)
{
    if (auto* param = p.apvts.getParameter (id))
        param->setValueNotifyingHost (param->convertTo0to1 (value));
    else
        std::printf ("    (unknown parameter %s)\n", id);
}

bool chordInKey (const spn::Chord& c, int rootPc, int scaleIndex)
{
    const auto& sc = spn::scaleDef (scaleIndex);
    for (int i = 0; i < c.count; ++i)
    {
        const int rel = (((c.midi[i] - rootPc) % 12) + 12) % 12;
        bool ok = false;
        for (int s = 0; s < sc.count; ++s)
            ok = ok || sc.semis[s] == rel;
        if (! ok) return false;
    }
    return true;
}

Result run (const Setup& s, int block)
{
    Result r;
    r.block = block;

    SpenningurProcessor proc;
    if (s.monoLayout)
    {
        juce::AudioProcessor::BusesLayout l;
        l.inputBuses.add (juce::AudioChannelSet::mono());
        l.outputBuses.add (juce::AudioChannelSet::stereo());
        hardCheck (proc.setBusesLayout (l), "mono-in/stereo-out layout rejected");
    }
    proc.setRateAndBufferSizeDetails (kSr, block);
    if (s.params) s.params (proc);
    proc.prepareToPlay (kSr, block);

    const std::vector<float> in = makeMelody (s.sig);
    const int total = (int) in.size();
    const int numIn = proc.getTotalNumInputChannels();
    const float rs = numIn > 1 ? s.rScale : 1.0f;

    const int scaleIndex = (int) std::lround (proc.apvts.getRawParameterValue (spn::pid::scale)->load());

    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;

    double sum = 0.0, tailSum = 0.0, bodySum = 0.0;
    long n = 0, tailN = 0, bodyN = 0;
    const int tailFrom = total - (int) kSr;                               // last second
    const int bodyFrom = (int) (kLead * kSr), bodyTo = (int) ((kLead + 8 * kSlot) * kSr);
    uint32_t lastSerial = proc.live.chordSerial.load();
    int lastNote = -1;
    double cpuMs = 0.0;

    for (int pos = 0; pos < total; pos += block)
    {
        const int cnt = std::min (block, total - pos);
        for (int i = 0; i < cnt; ++i)
        {
            buf.setSample (0, i, in[(size_t) (pos + i)]);
            // On a mono layout the second channel holds junk the processor must never read.
            buf.setSample (1, i, numIn > 1 ? in[(size_t) (pos + i)] * rs : 0.77f);
        }

        juce::AudioBuffer<float> view (buf.getArrayOfWritePointers(), 2, 0, cnt);
        const double t0 = juce::Time::getMillisecondCounterHiRes();
        proc.processBlock (view, midi);
        cpuMs += juce::Time::getMillisecondCounterHiRes() - t0;

        for (int i = 0; i < cnt; ++i)
        {
            const float inL = in[(size_t) (pos + i)];
            const float inR = numIn > 1 ? inL * rs : inL;
            const float oL = buf.getSample (0, i), oR = buf.getSample (1, i);
            if (! std::isfinite (oL) || ! std::isfinite (oR))
            {
                r.finite = false;
                continue;
            }
            r.peak = std::max (r.peak, std::max (std::abs (oL), std::abs (oR)));
            r.maxDiff = std::max (r.maxDiff, std::max (std::abs (oL - inL), std::abs (oR - inR)));
            const double e = 0.5 * ((double) oL * oL + (double) oR * oR);
            sum += e; ++n;
            if (pos + i >= tailFrom) { tailSum += e; ++tailN; }
            if (pos + i >= bodyFrom && pos + i < bodyTo) { bodySum += e; ++bodyN; }
        }

        const int dm = proc.live.detectedMidi.load();
        if (dm >= 0 && dm != lastNote) r.notes.push_back (dm);
        lastNote = dm;

        const uint32_t ser = proc.live.chordSerial.load();
        if (ser != lastSerial)
        {
            spn::Chord c;
            c.count = proc.live.chordCount.load();
            c.rootPc = proc.live.chordRootPc.load();
            c.style = (spn::ChordStyle) proc.live.chordStyle.load();
            for (int k = 0; k < c.count && k < spn::kMaxVoices; ++k)
                c.midi[k] = proc.live.chordMidi[k].load();
            const int eff = proc.live.effectiveRootPc.load();
            r.chords.push_back ({ c.rootPc, (int) c.style, eff, chordInKey (c, eff, scaleIndex) });
            r.numChords += (int) (ser - lastSerial);
            lastSerial = ser;
        }

        r.maxGr = std::max (r.maxGr, proc.live.limGrDb.load());
        r.maxEnv = std::max (r.maxEnv, proc.live.envLevel.load());
        r.gateSeen = r.gateSeen || proc.live.gateOpen.load();
        r.finalRoot = proc.live.effectiveRootPc.load();
    }

    r.rms = (float) std::sqrt (sum / std::max (1L, n));
    r.tailRms = (float) std::sqrt (tailSum / std::max (1L, tailN));
    r.bodyRms = (float) std::sqrt (bodySum / std::max (1L, bodyN));
    r.cpuPct = 100.0 * cpuMs / (1000.0 * total / kSr);

    for (int m : r.notes) r.noteText += noteName (m) + " ";
    for (auto& c : r.chords)
        r.chordText += std::string (spn::rootName (c.rootPc)) + ":" + spn::styleName ((spn::ChordStyle) c.style) + " ";
    return r;
}

/** How many of the played notes appear, in order, in the detected sequence. */
int matchedInOrder (const std::vector<int>& det)
{
    size_t d = 0;
    int hit = 0;
    for (int i = 0; i < 8; ++i)
        for (size_t k = d; k < det.size(); ++k)
            if (det[k] == kLine[i]) { ++hit; d = k + 1; break; }
    return hit;
}

void print (const Setup& s, const Result& r)
{
    std::printf ("%-26s b=%-3d notes %zu [%s] chords %d [%s]\n", s.name.c_str(), r.block, r.notes.size(),
                 r.noteText.c_str(), r.numChords, r.chordText.c_str());
    std::printf ("%-26s       rms %.4f  peak %.4f  tail %.5f  gr %.2f dB  env %.2f  cpu %.2f%%  %s\n", "", r.rms, r.peak,
                 r.tailRms, r.maxGr, r.maxEnv, r.cpuPct, r.finite ? "finite" : "*** NOT FINITE ***");
}

/** Run a scenario at every block size, print it, apply the universal hard checks. */
std::vector<Result> go (const Setup& s, bool limOnForPeakCheck = true)
{
    std::vector<Result> out;
    for (int b : s.blocks)
    {
        Result r = run (s, b);
        print (s, r);
        hardCheck (r.finite, s.name + ": NaN/inf in output");
        if (limOnForPeakCheck)
            hardCheck (r.peak <= 1.5f, s.name + ": output peak " + std::to_string (r.peak) + " > 1.5 with LIM on");
        out.push_back (std::move (r));
    }
    return out;
}

Setup mk (const char* name, Sig sig, std::function<void (SpenningurProcessor&)> params = {})
{
    Setup s;
    s.name = name;
    s.sig = sig;
    s.params = std::move (params);
    return s;
}

// ---------------------------------------------------------------- edge cases

void edgeCases()
{
    std::printf ("\n-- edge cases: odd block sizes, empty blocks, sample-rate changes\n");
    const double rates[3] = { 44100.0, 96000.0, 48000.0 };
    const int sizes[8] = { 0, 1, 7, 64, 255, 257, 1000, 4096 };

    SpenningurProcessor proc;
    proc.setRateAndBufferSizeDetails (kSr, 512);
    juce::MidiBuffer midi;
    bool finite = true;
    for (double rate : rates)
    {
        proc.setRateAndBufferSizeDetails (rate, 512);
        proc.prepareToPlay (rate, 512);          // blocks of 1000 / 4096 exceed this on purpose
        double ph = 0.0;
        for (int rep = 0; rep < 40; ++rep)
            for (int sz : sizes)
            {
                juce::AudioBuffer<float> b (2, std::max (1, sz));
                juce::AudioBuffer<float> view (b.getArrayOfWritePointers(), 2, 0, sz);
                for (int i = 0; i < sz; ++i)
                {
                    const float v = (float) (0.3 * std::sin (ph));
                    ph += kPi2 * 220.0 / rate;
                    b.setSample (0, i, v);
                    b.setSample (1, i, v);
                }
                proc.processBlock (view, midi);
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < sz; ++i)
                        finite = finite && std::isfinite (b.getSample (c, i));
            }
        std::printf ("  rate %.0f: ok\n", rate);
    }
    hardCheck (finite, "edge cases: NaN/inf in output");

    // processBlock before prepareToPlay must not crash or emit garbage.
    SpenningurProcessor cold;
    juce::AudioBuffer<float> b (2, 64);
    b.clear();
    cold.processBlock (b, midi);
    std::printf ("  unprepared processBlock: ok\n");

    // Parameter hammering from a block loop (RANGE switching must be safe).
    SpenningurProcessor ham;
    ham.setRateAndBufferSizeDetails (kSr, 128);
    ham.prepareToPlay (kSr, 128);
    const auto x = makeMelody (Sig::Saw);
    bool hf = true;
    juce::AudioBuffer<float> hb (2, 128);
    for (int blk = 0; blk * 128 + 128 <= (int) x.size(); ++blk)
    {
        if (blk % 20 == 0) set (ham, spn::pid::range, (float) ((blk / 20) % 3));
        if (blk % 33 == 0) set (ham, spn::pid::scale, (float) ((blk / 33) % spn::kNumScales));
        if (blk % 17 == 0) set (ham, spn::pid::voices, (float) ((blk / 17) % 5));
        if (blk % 29 == 0) set (ham, spn::pid::track, (float) ((blk / 29) % 2));
        for (int i = 0; i < 128; ++i)
        {
            hb.setSample (0, i, x[(size_t) (blk * 128 + i)]);
            hb.setSample (1, i, x[(size_t) (blk * 128 + i)]);
        }
        ham.processBlock (hb, midi);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 128; ++i)
                hf = hf && std::isfinite (hb.getSample (c, i));
    }
    std::printf ("  parameter hammering (RANGE/SCALE/VOICES/TRACK switching): %s\n", hf ? "finite" : "*** NOT FINITE ***");
    hardCheck (hf, "parameter hammering: NaN/inf in output");
}
} // namespace

int main (int argc, char** argv)
{
    for (int i = 1; i < argc; ++i)
        if (! std::strcmp (argv[i], "--strict"))
            gStrict = true;

    juce::ScopedJuceInitialiser_GUI init;
    std::printf ("dsp_check%s  (%.0f Hz, blocks 64 and 512)\n\n", gStrict ? " --strict" : "", kSr);

    // ------------------------------------------------------------ defaults and timbres
    std::printf ("-- defaults on three timbres (A-minor line A3 C4 E4 D4 B3 F4 E4 A3, 0.5 s per note)\n");
    std::vector<Result> defaults;
    for (auto [name, sig] : { std::pair<const char*, Sig> { "defaults, sine", Sig::Sine },
                              { "defaults, saw", Sig::Saw },
                              { "defaults, vibrato", Sig::Vibrato } })
    {
        auto rs = go (mk (name, sig));
        for (auto& r : rs)
        {
            softCheck (matchedInOrder (r.notes) >= 6, std::string (name) + ": only " + std::to_string (matchedInOrder (r.notes))
                                                          + "/8 notes detected");
            softCheck (r.numChords >= 3, std::string (name) + ": fewer than 3 chord changes");
            softCheck (r.maxEnv > 0.0f, std::string (name) + ": envelope never opened");
            softCheck (r.gateSeen, std::string (name) + ": gate never opened");
            bool allInKey = true;
            for (auto& c : r.chords) allInKey = allInKey && c.inKey;
            softCheck (allInKey, std::string (name) + ": a chord had tones outside the scale");
        }
        if (sig == Sig::Sine) defaults = rs;
    }
    if (! defaults.empty() && defaults[0].notes.empty())
        std::printf ("\nNOTE: the detector reported no notes at all - the core is probably still stubbed.\n"
                     "      Detector-dependent checks are reported as NOTE; use --strict once the core lands.\n\n");
    if (defaults.size() == 2)
        softCheck (defaults[0].chordText == defaults[1].chordText && defaults[0].noteText == defaults[1].noteText,
                   "defaults: notes/chords differ between block 64 and block 512");

    // ------------------------------------------------------------ RANDOM off, each style
    std::printf ("\n-- RANDOM off, each style (TRACK off, root A, minor, 5 voices)\n");
    for (int st = 0; st < spn::kNumStyles; ++st)
    {
        std::string name = std::string ("style ") + spn::styleName ((spn::ChordStyle) st);
        auto rs = go (mk (name.c_str(), Sig::Sine, [st] (SpenningurProcessor& p)
        {
            set (p, spn::pid::random, 0.0f);
            set (p, spn::pid::chord, (float) st);
        }));
        for (auto& r : rs)
        {
            bool allSame = ! r.chords.empty();
            for (auto& c : r.chords) allSame = allSame && c.style == st;
            softCheck (allSame, name + ": chords not all of the requested style");
            softCheck (r.maxEnv > 0.0f || r.rms > 1e-4f, name + ": silent");
        }
    }

    // ------------------------------------------------------------ voices
    std::printf("\n-- VOICES 2 and 6\n");
    for (int v : { 0, 4 })
    {
        std::string name = "voices " + std::to_string (v + 2);
        go (mk (name.c_str(), Sig::Saw, [v] (SpenningurProcessor& p) { set (p, spn::pid::voices, (float) v); }));
    }

    // ------------------------------------------------------------ TRACK
    std::printf ("\n-- TRACK on (RANDOM off, 7th, minor): root follows the input\n");
    for (int follows = 0; follows < 2; ++follows)
    {
        std::string name = follows == 0 ? "track NOTE" : "track KEY";
        auto rs = go (mk (name.c_str(), Sig::Sine, [follows] (SpenningurProcessor& p)
        {
            set (p, spn::pid::random, 0.0f);
            set (p, spn::pid::track, 1.0f);
            set (p, spn::pid::follows, (float) follows);
            set (p, spn::pid::root, 0.0f);      // hand root deliberately wrong (C): tracking must override it
        }));
        for (auto& r : rs)
        {
            std::printf ("%-26s       effective root at end: %s\n", "", r.finalRoot >= 0 ? spn::rootName (r.finalRoot) : "?");
            softCheck (r.numChords >= 3, name + ": fewer than 3 chord changes");
            if (follows == 1)
                softCheck (r.finalRoot == 9, name + ": key did not settle on A (got "
                                                  + std::string (r.finalRoot >= 0 ? spn::rootName (r.finalRoot) : "?") + ")");
            else
                softCheck (r.finalRoot == 9, name + ": last note was A3, root should be A");
        }
    }

    // ------------------------------------------------------------ RANGE
    std::printf ("\n-- each RANGE (saw)\n");
    for (int rg = 0; rg < 3; ++rg)
    {
        std::string name = std::string ("range ") + (rg == 0 ? "SMALL" : rg == 1 ? "MEDIUM" : "LARGE");
        auto rs = go (mk (name.c_str(), Sig::Saw, [rg] (SpenningurProcessor& p) { set (p, spn::pid::range, (float) rg); }));
        for (auto& r : rs)
            softCheck (matchedInOrder (r.notes) >= 6, name + ": only " + std::to_string (matchedInOrder (r.notes)) + "/8 notes");
    }

    // ------------------------------------------------------------ ADSR
    std::printf ("\n-- ADSR: transient vs pad (MIX 100%%, RANDOM off)\n");
    auto transient = go (mk ("adsr transient", Sig::Saw, [] (SpenningurProcessor& p)
    {
        set (p, spn::pid::mix, 1.0f); set (p, spn::pid::random, 0.0f);
        set (p, spn::pid::atk, 1.0f); set (p, spn::pid::dec, 150.0f); set (p, spn::pid::sus, 0.0f); set (p, spn::pid::rel, 200.0f);
    }));
    auto pad = go (mk ("adsr pad", Sig::Saw, [] (SpenningurProcessor& p)
    {
        set (p, spn::pid::mix, 1.0f); set (p, spn::pid::random, 0.0f);
        set (p, spn::pid::atk, 600.0f); set (p, spn::pid::dec, 400.0f); set (p, spn::pid::sus, 0.8f); set (p, spn::pid::rel, 3000.0f);
    }));
    for (size_t i = 0; i < transient.size() && i < pad.size(); ++i)
    {
        softCheck (transient[i].rms > 1e-4f && pad[i].rms > 1e-4f, "adsr: wet output silent at MIX 100%");
        std::printf ("%-26s       body rms: transient %.4f vs pad %.4f   tail rms: transient %.5f vs pad %.5f\n", "",
                     transient[i].bodyRms, pad[i].bodyRms, transient[i].tailRms, pad[i].tailRms);
    }

    // ------------------------------------------------------------ MIX
    std::printf ("\n-- MIX 0: output must equal the input exactly (stereo, L != R)\n");
    {
        Setup s = mk ("mix 0 (stereo)", Sig::Saw, [] (SpenningurProcessor& p) { set (p, spn::pid::mix, 0.0f); });
        s.rScale = 0.7f;
        for (auto& r : go (s))
            hardCheck (r.maxDiff < 1e-6f, "mix 0 not transparent (max diff " + std::to_string (r.maxDiff) + ")");
    }
    std::printf ("\n-- MIX 0, antiphase input (mono sum is zero): still exactly the input\n");
    {
        Setup s = mk ("mix 0 (antiphase)", Sig::Sine, [] (SpenningurProcessor& p) { set (p, spn::pid::mix, 0.0f); });
        s.rScale = -1.0f;
        for (auto& r : go (s))
            hardCheck (r.maxDiff < 1e-6f, "mix 0 antiphase not transparent (max diff " + std::to_string (r.maxDiff) + ")");
    }
    std::printf ("\n-- MIX 100%%, antiphase input: the resonator hears (L+R)/2 = 0, so any output is dry leaking\n");
    {
        Setup s = mk ("mix 1 (antiphase)", Sig::Sine, [] (SpenningurProcessor& p) { set (p, spn::pid::mix, 1.0f); });
        s.rScale = -1.0f;
        for (auto& r : go (s))
            hardCheck (r.rms < 1e-4f && r.peak < 1e-3f, "mix 1 leaks dry (rms " + std::to_string (r.rms) + ")");
    }
    std::printf ("\n-- MIX 100%%, normal input\n");
    for (auto& r : go (mk ("mix 1", Sig::Saw, [] (SpenningurProcessor& p) { set (p, spn::pid::mix, 1.0f); })))
        softCheck (r.rms > 1e-4f, "mix 1: wet path silent");

    // ------------------------------------------------------------ LIM
    std::printf ("\n-- LIM with a hot input (full-scale saw, IN +12 dB, MIX 100%%, RING 8 s, sustain 100%%)\n");
    auto hotSetup = [] (bool lim)
    {
        return [lim] (SpenningurProcessor& p)
        {
            set (p, spn::pid::mix, 1.0f); set (p, spn::pid::in, 12.0f); set (p, spn::pid::ring, 8.0f);
            set (p, spn::pid::sus, 1.0f); set (p, spn::pid::lim, lim ? 1.0f : 0.0f);
        };
    };
    for (auto& r : go (mk ("lim on, hot", Sig::Hot, hotSetup (true))))
    {
        softCheck (r.maxGr > 0.0f, "lim on, hot: limiter never reduced gain");
        softCheck (r.peak > 1e-3f, "lim on, hot: silent");
    }
    for (auto& r : go (mk ("lim off, hot", Sig::Hot, hotSetup (false)), false))
    {
        std::printf ("%-26s       (limiter off: peak %.3f is information only)\n", "", r.peak);
        hardCheck (r.maxGr == 0.0f, "lim off: gain reduction reported");
    }

    // ------------------------------------------------------------ mono layout
    std::printf ("\n-- mono input, stereo output (the unused second channel carries junk)\n");
    {
        Setup s = mk ("mono in, mix 0", Sig::Saw, [] (SpenningurProcessor& p) { set (p, spn::pid::mix, 0.0f); });
        s.monoLayout = true;
        for (auto& r : go (s))
            hardCheck (r.maxDiff < 1e-6f, "mono layout, mix 0: both outputs should equal the input (max diff "
                                              + std::to_string (r.maxDiff) + ")");
        Setup d = mk ("mono in, defaults", Sig::Sine);
        d.monoLayout = true;
        for (auto& r : go (d))
            softCheck (matchedInOrder (r.notes) >= 6, "mono in: notes not detected");
        // Same material, same result as stereo with identical channels.
        Setup m1 = mk ("mono in, mix 1", Sig::Saw, [] (SpenningurProcessor& p) { set (p, spn::pid::mix, 1.0f); });
        m1.monoLayout = true;
        m1.blocks = { 512 };
        Setup s1 = mk ("stereo in, mix 1", Sig::Saw, [] (SpenningurProcessor& p) { set (p, spn::pid::mix, 1.0f); });
        s1.blocks = { 512 };
        auto a = go (m1), b = go (s1);
        softCheck (std::abs (a[0].rms - b[0].rms) < 1e-5f, "mono and identical-stereo input should render the same wet");
    }

    // ------------------------------------------------------------ edge cases
    edgeCases();

    std::printf ("\n%s: %d hard failure(s), %d soft note(s)%s\n", gHardFails ? "FAILED" : "OK", gHardFails, gSoftFails,
                 gStrict ? " (strict)" : "");
    return gHardFails ? 1 : 0;
}
