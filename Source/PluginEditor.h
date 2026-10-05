#pragma once

#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "Panel.h"
#include "PluginProcessor.h"

/**
 * The panel: every control on one surface. Drawing and hit testing both work in
 * the fixed 640 x 744 design space of Panel.h, with one scale transform applied
 * on the way out, so the layout constants are the only source of truth.
 *
 * Every control is bound to its parameter by ID (ParamIDs.h) and writes through
 * begin/endChangeGesture, so host automation sees one gesture per drag or click.
 * A 30 Hz timer repaints from the processor's LiveState.
 */
class SpenningurEditor final : public juce::AudioProcessorEditor,
                               private juce::Timer
{
public:
    explicit SpenningurEditor (SpenningurProcessor&);
    ~SpenningurEditor() override;

    void paint (juce::Graphics&) override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    /** Reads LiveState: chord history, meter ballistics. Called by the timer;
        public so the offscreen renderer can drive it without a message loop. */
    void pollLive();

private:
    void timerCallback() override { pollLive(); repaint(); }

    enum class Fmt { inDb, gateDb, percent, ms, seconds, kHz, oct, wet, envMs };

    struct KnobCtl
    {
        juce::RangedAudioParameter* param = nullptr;
        float cx = 0, top = 0, r = 0, travel = 80.0f;
        juce::Colour colour;
        const char* label = "";
        Fmt fmt = Fmt::percent;
        juce::Rectangle<float> hit;
    };

    struct FaderCtl
    {
        juce::RangedAudioParameter* param = nullptr;
        float x = 0, top = 457.0f;
        const char* label = "";
        Fmt fmt = Fmt::envMs;
        juce::Rectangle<float> hit;
    };

    struct LatchCtl
    {
        juce::RangedAudioParameter* param = nullptr;
        juce::Rectangle<float> rect;
        const char* label = "";
        juce::Colour colour;
    };

    struct SelectorCtl
    {
        juce::RangedAudioParameter* param = nullptr;
        juce::StringArray options;
        spn::panel::SelectorLayout layout;
        juce::Colour colour;
        bool vertical = false;
    };

    struct DropCtl
    {
        juce::RangedAudioParameter* param = nullptr;
        juce::StringArray options;
        juce::Rectangle<float> rect;
    };

    struct ChordRec
    {
        int rootPc = 0, style = 0, count = 0;
        int midi[6] = {};
    };

    enum Drop { dRoot = 0, dScale, dChord, numDrops };

    void buildControls();
    void openDropdown (int which);

    float scale() const;
    juce::Point<float> toDesign (juce::Point<float> px) const;

    juce::String readout (const KnobCtl&) const;
    juce::String readout (const FaderCtl&) const;
    juce::String format (Fmt, float value) const;

    int  choiceIndex (const juce::RangedAudioParameter&) const;
    bool boolValue (const juce::RangedAudioParameter&) const;
    void setChoice (juce::RangedAudioParameter&, int index);
    void toggle (juce::RangedAudioParameter&);

    juce::RangedAudioParameter* param (const char* id) const;

    SpenningurProcessor& proc;
    spn::panel::MenuLookAndFeel menuLnf;

    std::vector<KnobCtl>     knobs;
    std::vector<FaderCtl>    faders;
    std::vector<LatchCtl>    latches;
    std::vector<SelectorCtl> selectors;
    DropCtl                  drops[numDrops];

    enum Sel { sRange = 0, sFollows, sVoices, sMode, numSels };
    enum Lat { lTrack = 0, lRandom, lLim };

    // Live
    std::vector<ChordRec> history;      // oldest first, at most four
    uint32_t lastSerial = 0;
    float inDisp = 0.0f, outDisp = 0.0f;

    // Interaction
    enum class Drag { none, knob, fader };
    Drag dragKind = Drag::none;
    int dragIdx = -1;
    float dragStartNorm = 0.0f, dragTravel = 80.0f;
    juce::Point<float> dragStart;
    int openDrop = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpenningurEditor)
};
