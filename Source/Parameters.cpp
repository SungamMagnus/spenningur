#include "Parameters.h"

#include "ParamIDs.h"
#include "spenningur/Harmony.h"

namespace spn
{
namespace
{
using Attrs = juce::AudioParameterFloatAttributes;

juce::ParameterID id (const char* s) { return { s, 1 }; }

std::unique_ptr<juce::AudioParameterFloat> flt (const char* pid, const char* name, float lo, float hi, float def,
                                                const char* unit, float centre = -1.0f)
{
    juce::NormalisableRange<float> r (lo, hi);
    if (centre > 0.0f) r.setSkewForCentre (centre);
    return std::make_unique<juce::AudioParameterFloat> (id (pid), name, r, def, Attrs().withLabel (unit));
}

std::unique_ptr<juce::AudioParameterChoice> choice (const char* pid, const char* name, juce::StringArray items, int def)
{
    return std::make_unique<juce::AudioParameterChoice> (id (pid), name, items, def);
}

std::unique_ptr<juce::AudioParameterBool> toggle (const char* pid, const char* name, bool def)
{
    return std::make_unique<juce::AudioParameterBool> (id (pid), name, def);
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;

    // Input
    l.add (flt (pid::in,   "In",   -12.0f, 12.0f, 0.0f, "dB"));
    l.add (flt (pid::gate, "Gate", -72.0f, -24.0f, -58.0f, "dB"));
    l.add (choice (pid::range, "Range", { "SMALL", "MEDIUM", "LARGE" }, 1));

    // Harmony
    juce::StringArray notes, scales, styles;
    for (int i = 0; i < 12; ++i) notes.add (rootName (i));
    for (int i = 0; i < kNumScales; ++i) scales.add (scaleDef (i).name);
    for (int i = 0; i < kNumStyles; ++i) styles.add (styleName ((ChordStyle) i));
    l.add (choice (pid::root, "Root", notes, 9));
    l.add (toggle (pid::track, "Track", false));
    l.add (choice (pid::follows, "Follows", { "NOTE", "KEY" }, 1));
    l.add (choice (pid::scale, "Scale", scales, 1));
    l.add (choice (pid::chord, "Chord", styles, 1));
    l.add (toggle (pid::random, "Random", true));
    l.add (choice (pid::voices, "Voices", { "2", "3", "4", "5", "6" }, 3));

    // Progression
    l.add (choice (pid::mode, "Mode", { "FOLLOW", "CADENCE", "WANDER" }, 1));
    l.add (flt (pid::tension, "Tension", 0.0f, 1.0f, 0.35f, ""));
    l.add (flt (pid::hold, "Hold", 50.0f, 1000.0f, 300.0f, "ms"));

    // Resonator
    l.add (flt (pid::ring,   "Ring",    0.2f, 8.0f, 4.5f, "s", 2.0f));
    l.add (flt (pid::damp,   "Damp",    1000.0f, 12000.0f, 6500.0f, "Hz", 4000.0f));
    l.add (flt (pid::spread, "Spread",  0.0f, 3.0f, 1.2f, "oct"));
    l.add (flt (pid::glide,  "Glide",   0.0f, 200.0f, 40.0f, "ms"));
    l.add (flt (pid::atk,    "Attack",  1.0f, 2000.0f, 30.0f, "ms", 150.0f));
    l.add (flt (pid::dec,    "Decay",   10.0f, 2000.0f, 413.0f, "ms", 300.0f));
    l.add (flt (pid::sus,    "Sustain", 0.0f, 1.0f, 0.6f, ""));
    l.add (flt (pid::rel,    "Release", 20.0f, 8000.0f, 2000.0f, "ms", 800.0f));

    // Output
    l.add (flt (pid::mix, "Mix", 0.0f, 1.0f, 0.6f, ""));
    l.add (toggle (pid::lim, "Limiter", true));

    return l;
}
} // namespace spn
