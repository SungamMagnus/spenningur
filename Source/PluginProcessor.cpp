#include "PluginProcessor.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "ParamIDs.h"
#include "Parameters.h"
#include "PluginEditor.h"
#include "spenningur/Harmony.h"
#include "spenningur/Pitch.h"
#include "spenningur/Resonator.h"

namespace
{
constexpr uint32_t kSeed = 0x5eed1234u;       // fixed so a render is repeatable
constexpr float    kWetClamp = 16.0f;         // backstop only; the limiter does the real work

inline float finiteOr0 (float v) noexcept { return std::isfinite (v) ? v : 0.0f; }
} // namespace

struct SpenningurProcessor::Impl
{
    // Raw parameter values, cached once.
    std::atomic<float> *pIn, *pGate, *pRange, *pRoot, *pTrack, *pFollows, *pScale, *pChord, *pRandom, *pVoices,
        *pMode, *pTension, *pHold, *pRing, *pDamp, *pSpread, *pGlide, *pAtk, *pDec, *pSus, *pRel, *pMix, *pLim;

    // Core objects. Three detectors are prepared up front so RANGE never allocates.
    spn::PitchDetector detectors[3];
    spn::NoteTracker   tracker;
    spn::ChordEngine   engine;
    spn::Resonator     resonator;
    spn::Limiter       limiter;

    // Scratch, sized in prepareToPlay.
    std::vector<float> mono, wetL, wetR;
    int hop = 256;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> inGain, mixSm;

    double sampleRate = 48000.0;
    double samplesElapsed = 0.0;       // running sample counter -> monotonic ms clock
    bool   prepared = false;
    int    activeRange = -1;
    bool   gateOpen = false;

    float inLevel = 0.0f, outLevel = 0.0f;
    float levelRelease = 0.9999f;      // per sample

    explicit Impl (juce::AudioProcessorValueTreeState& s)
    {
        auto raw = [&s] (const char* id)
        {
            auto* p = s.getRawParameterValue (id);
            jassert (p != nullptr);
            return p;
        };
        pIn = raw (spn::pid::in);           pGate = raw (spn::pid::gate);       pRange = raw (spn::pid::range);
        pRoot = raw (spn::pid::root);       pTrack = raw (spn::pid::track);     pFollows = raw (spn::pid::follows);
        pScale = raw (spn::pid::scale);     pChord = raw (spn::pid::chord);     pRandom = raw (spn::pid::random);
        pVoices = raw (spn::pid::voices);   pMode = raw (spn::pid::mode);       pTension = raw (spn::pid::tension);
        pHold = raw (spn::pid::hold);       pRing = raw (spn::pid::ring);       pDamp = raw (spn::pid::damp);
        pSpread = raw (spn::pid::spread);   pGlide = raw (spn::pid::glide);     pAtk = raw (spn::pid::atk);
        pDec = raw (spn::pid::dec);         pSus = raw (spn::pid::sus);         pRel = raw (spn::pid::rel);
        pMix = raw (spn::pid::mix);         pLim = raw (spn::pid::lim);
    }

    static int idx (const std::atomic<float>* p, int maxIdx) noexcept
    {
        return juce::jlimit (0, maxIdx, (int) std::lround (p->load (std::memory_order_relaxed)));
    }
    static float val (const std::atomic<float>* p) noexcept { return p->load (std::memory_order_relaxed); }

    void prepare (double sr, int samplesPerBlock)
    {
        sampleRate = sr > 0.0 ? sr : 48000.0;
        const spn::Range ranges[3] = { spn::Range::Small, spn::Range::Medium, spn::Range::Large };
        for (int i = 0; i < 3; ++i)
            detectors[i].prepare (sampleRate, ranges[i]);

        hop = std::max (1, detectors[1].hopSamples());
        tracker.prepare (detectors[1].hopRateHz());
        tracker.reset();

        const int maxBlock = std::max (std::max (1, samplesPerBlock), hop);
        mono.assign ((size_t) maxBlock, 0.0f);
        wetL.assign ((size_t) maxBlock, 0.0f);
        wetR.assign ((size_t) maxBlock, 0.0f);

        resonator.prepare (sampleRate, maxBlock);
        resonator.reset();
        limiter.prepare (sampleRate);
        limiter.reset();
        engine.prepare (kSeed);
        engine.reset();

        inGain.reset (sampleRate, 0.010);
        inGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (val (pIn)));
        mixSm.reset (sampleRate, 0.020);
        mixSm.setCurrentAndTargetValue (val (pMix));

        samplesElapsed = 0.0;
        activeRange = idx (pRange, 2);
        detectors[activeRange].reset();
        gateOpen = false;
        inLevel = outLevel = 0.0f;
        levelRelease = (float) std::exp (-1.0 / (sampleRate * 0.35));
        prepared = true;
    }

    spn::HarmonyParams harmonyParams() const
    {
        spn::HarmonyParams h;
        h.rootPc      = idx (pRoot, 11);
        h.trackRoot   = val (pTrack) >= 0.5f;
        h.follows     = (spn::TrackMode) idx (pFollows, 1);
        h.scaleIndex  = idx (pScale, spn::kNumScales - 1);
        h.style       = (spn::ChordStyle) idx (pChord, spn::kNumStyles - 1);
        h.randomStyle = val (pRandom) >= 0.5f;
        h.voices      = idx (pVoices, 4) + 2;
        h.mode        = (spn::ProgMode) idx (pMode, 2);
        h.tension     = juce::jlimit (0.0f, 1.0f, val (pTension));
        h.holdMs      = val (pHold);
        return h;
    }

    spn::ResonatorParams resonatorParams() const
    {
        spn::ResonatorParams r;
        r.ringSec   = val (pRing);
        r.dampHz    = val (pDamp);
        r.spreadOct = val (pSpread);
        r.glideMs   = val (pGlide);
        r.attackMs  = val (pAtk);
        r.decayMs   = val (pDec);
        r.sustain   = val (pSus);
        r.releaseMs = val (pRel);
        return r;
    }
};

namespace
{
inline void publishChord (LiveState& live, const spn::Chord& c)
{
    live.chordRootPc.store (c.rootPc, std::memory_order_relaxed);
    live.chordStyle.store ((int) c.style, std::memory_order_relaxed);
    live.chordCount.store (c.count, std::memory_order_relaxed);
    for (int i = 0; i < spn::kMaxVoices; ++i)
        live.chordMidi[i].store (i < c.count ? c.midi[i] : 0, std::memory_order_relaxed);
    live.chordSerial.fetch_add (1, std::memory_order_release);
}
} // namespace

SpenningurProcessor::SpenningurProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", spn::createParameterLayout())
{
    impl = std::make_unique<Impl> (apvts);
}

SpenningurProcessor::~SpenningurProcessor() = default;

void SpenningurProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    impl->prepare (sampleRate, samplesPerBlock);
    setLatencySamples (0);      // detection only steers the chord; the dry path is untouched
}

bool SpenningurProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet();
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

void SpenningurProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto& m = *impl;
    const int numSamples = buffer.getNumSamples();
    const int numIn      = getTotalNumInputChannels();
    const int numCh      = buffer.getNumChannels();

    for (int ch = numIn; ch < numCh; ++ch)
        buffer.clear (ch, 0, numSamples);

    if (numSamples <= 0 || numCh <= 0)
        return;

    if (! m.prepared || numIn <= 0)
    {
        // Not prepared (or no input): stay silent rather than touch unallocated state.
        buffer.clear();
        return;
    }

    // Parameters, once per block.
    const float gateDb = Impl::val (m.pGate);
    const int   range  = Impl::idx (m.pRange, 2);
    const bool  limOn  = Impl::val (m.pLim) >= 0.5f;
    const spn::HarmonyParams hp = m.harmonyParams();

    m.tracker.setGateDb (gateDb);
    m.resonator.setParams (m.resonatorParams());
    m.limiter.setEnabled (limOn);
    m.inGain.setTargetValue (juce::Decibels::decibelsToGain (Impl::val (m.pIn)));
    m.mixSm.setTargetValue (juce::jlimit (0.0f, 1.0f, Impl::val (m.pMix)));

    if (range != m.activeRange)
    {
        // Another RANGE: start its detector from a clean window and let go of any held note.
        m.activeRange = range;
        m.detectors[range].reset();
        if (m.tracker.isHeld())
            m.resonator.noteOff();
        m.tracker.reset();
    }
    spn::PitchDetector& det = m.detectors[range];

    float* chL = buffer.getWritePointer (0);
    float* chR = numCh > 1 ? buffer.getWritePointer (1) : nullptr;
    const float* srcR = numIn > 1 ? chR : chL;       // mono input: right dry is the left

    const float gateLin  = juce::Decibels::decibelsToGain (gateDb);
    const float gateOffL = juce::Decibels::decibelsToGain (gateDb - 3.0f);
    float inPeak = 0.0f, outPeak = 0.0f, grMax = 0.0f;

    int pos = 0;
    while (pos < numSamples)
    {
        const int n = std::min (numSamples - pos, m.hop);

        // 2. Mono excitation, scaled by IN. Dry stays untouched in the buffer for now.
        float chunkPeak = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float l = chL[pos + i];
            const float r = srcR[pos + i];
            const float mo = numIn > 1 ? 0.5f * (l + r) : l;
            inPeak = std::max (inPeak, std::max (std::abs (l), std::abs (r)));
            const float x = finiteOr0 (mo) * m.inGain.getNextValue();
            chunkPeak = std::max (chunkPeak, std::abs (x));
            m.mono[(size_t) i] = x;
        }
        m.samplesElapsed += n;
        const double nowMs = m.samplesElapsed * 1000.0 / m.sampleRate;

        // Gate lamp on the IN-scaled signal (peak * 0.707 ~ RMS of a sine), 3 dB hysteresis like the tracker's.
        m.gateOpen = chunkPeak * 0.707f > (m.gateOpen ? gateOffL : gateLin);

        // 3-4. Detect, track, choose chords; events take effect before this chunk is rung.
        spn::Estimate est[2];
        const int numEst = det.process (m.mono.data(), n, est, 2);
        for (int e = 0; e < numEst; ++e)
        {
            spn::NoteEvent ev[2];
            const int numEv = m.tracker.process (est[e], ev);
            for (int k = 0; k < numEv; ++k)
            {
                if (ev[k].type == spn::NoteEvent::Type::Off)
                {
                    // Off directly followed by On is a replaced note: keep the envelope held.
                    const bool replaced = k + 1 < numEv && ev[k + 1].type == spn::NoteEvent::Type::On;
                    if (! replaced)
                        m.resonator.noteOff();
                }
                else
                {
                    spn::Chord chord;
                    if (m.engine.onNote (ev[k].midiNote, nowMs, hp, chord))
                    {
                        m.resonator.setChord (chord.midi, chord.count);
                        m.resonator.noteOn();
                        publishChord (live, chord);
                    }
                }
            }
        }

        // 5. The bank, excited by the IN-scaled mono signal.
        m.resonator.process (m.mono.data(), m.wetL.data(), m.wetR.data(), n);

        // 6. Limiter on the wet path only.
        m.limiter.process (m.wetL.data(), m.wetR.data(), n);
        grMax = std::max (grMax, m.limiter.gainReductionDb());

        // Mix, in place: the buffer still holds this chunk's dry.
        for (int i = 0; i < n; ++i)
        {
            const float wet = m.mixSm.getNextValue();
            const float dry = 1.0f - wet;
            const float wl = juce::jlimit (-kWetClamp, kWetClamp, finiteOr0 (m.wetL[(size_t) i]));
            const float wr = juce::jlimit (-kWetClamp, kWetClamp, finiteOr0 (m.wetR[(size_t) i]));
            const float dl = chL[pos + i];
            const float dr = srcR[pos + i];
            const float ol = finiteOr0 (dl * dry + wl * wet);
            const float orr = finiteOr0 (dr * dry + wr * wet);
            chL[pos + i] = ol;
            if (chR != nullptr)
                chR[pos + i] = orr;
            outPeak = std::max (outPeak, std::max (std::abs (ol), std::abs (orr)));
        }

        pos += n;
    }

    // Extra output channels (never expected) are silent.
    for (int ch = 2; ch < numCh; ++ch)
        buffer.clear (ch, 0, numSamples);

    // 7. Publish.
    const float rel = std::pow (m.levelRelease, (float) numSamples);
    m.inLevel  = std::max (inPeak,  m.inLevel * rel);
    m.outLevel = std::max (outPeak, m.outLevel * rel);

    const auto relaxed = std::memory_order_relaxed;
    const bool held = m.tracker.isHeld();
    live.detectedMidi.store (held ? m.tracker.currentNote() : -1, relaxed);
    live.detectedCents.store (held ? m.tracker.currentCents() : 0.0f, relaxed);
    live.effectiveRootPc.store (hp.trackRoot ? m.engine.effectiveRootPc() : hp.rootPc, relaxed);
    live.inLevel.store (juce::jlimit (0.0f, 1.0f, m.inLevel), relaxed);
    live.outLevel.store (juce::jlimit (0.0f, 1.0f, m.outLevel), relaxed);
    live.limGrDb.store (limOn ? grMax : 0.0f, relaxed);
    live.envLevel.store (juce::jlimit (0.0f, 1.0f, finiteOr0 (m.resonator.envelopeLevel())), relaxed);
    live.gateOpen.store (m.gateOpen, relaxed);
}

juce::AudioProcessorEditor* SpenningurProcessor::createEditor() { return new SpenningurEditor (*this); }

void SpenningurProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void SpenningurProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SpenningurProcessor();
}
