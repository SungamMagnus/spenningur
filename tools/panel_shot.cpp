// Offscreen render of the real editor. Dev tool: verifies the panel without a
// host. Writes panel.png (TRACK + RANDOM on) and panel_manual.png (TRACK and
// RANDOM off) at 2x into the directory given as argv[1].
#include <juce_gui_basics/juce_gui_basics.h>

#include "ParamIDs.h"
#include "Panel.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
void setNorm (SpenningurProcessor& p, const char* id, float norm)
{
    if (auto* param = p.apvts.getParameter (id))
        param->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, norm));
}

/** Set a parameter in its own units (dB, ms, a choice index, 0/1). */
void setValue (SpenningurProcessor& p, const char* id, float value)
{
    if (auto* param = p.apvts.getParameter (id))
        param->setValueNotifyingHost (param->convertTo0to1 (value));
}

/** Publish one chord to LiveState the way the audio thread does, then let the editor see it. */
void pushChord (SpenningurProcessor& p, SpenningurEditor& ed, int rootPc, int style, std::initializer_list<int> midi)
{
    auto& l = p.live;
    int n = 0;
    for (int m : midi)
        l.chordMidi[n++].store (m);
    l.chordCount.store (n);
    l.chordRootPc.store (rootPc);
    l.chordStyle.store (style);
    l.chordSerial.fetch_add (1);
    ed.pollLive();
}

void writePng (SpenningurEditor& editor, const juce::File& out)
{
    juce::Image img (juce::Image::ARGB, editor.getWidth() * 2, editor.getHeight() * 2, true);
    juce::Graphics g (img);
    g.addTransform (juce::AffineTransform::scale (2.0f));
    editor.paintEntireComponent (g, false);

    juce::FileOutputStream stream (out);
    stream.setPosition (0);
    stream.truncate();
    juce::PNGImageFormat().writeImageToStream (img, stream);
    std::printf ("%s\n", out.getFullPathName().toRawUTF8());
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;

    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::String ("."));
    outDir.createDirectory();

    SpenningurProcessor proc;
    proc.setRateAndBufferSizeDetails (48000.0, 128);
    proc.prepareToPlay (48000.0, 128);

    namespace pid = spn::pid;

    /* Showcase: TRACK on in KEY mode, RANDOM on, A minor, five voices, a pad. */
    setValue (proc, pid::track, 1.0f);
    setValue (proc, pid::follows, 1.0f);
    setValue (proc, pid::random, 1.0f);
    setValue (proc, pid::scale, 1.0f);
    setValue (proc, pid::voices, 3.0f);                 // index 3 = five voices
    setValue (proc, pid::mode, 1.0f);
    setValue (proc, pid::range, 1.0f);
    setValue (proc, pid::atk, 600.0f);
    setValue (proc, pid::dec, 400.0f);
    setValue (proc, pid::sus, 0.8f);
    setValue (proc, pid::rel, 3000.0f);
    setValue (proc, pid::mix, 0.6f);
    setValue (proc, pid::lim, 1.0f);

    std::unique_ptr<juce::AudioProcessorEditor> base (proc.createEditor());
    auto* editor = dynamic_cast<SpenningurEditor*> (base.get());
    jassert (editor != nullptr);
    editor->setSize ((int) spn::panel::designW, (int) spn::panel::designH);

    /* A scripted line stands in for the audio thread. */
    auto& live = proc.live;
    live.detectedMidi.store (57);                       // A3
    live.detectedCents.store (4.0f);
    live.effectiveRootPc.store (9);                     // tracked: A
    live.gateOpen.store (true);
    live.limGrDb.store (0.0f);                          // limiter idle
    live.inLevel.store (std::pow (10.0f, (0.55f * 48.0f - 48.0f) / 20.0f));
    live.outLevel.store (std::pow (10.0f, (0.55f * 48.0f - 48.0f) / 20.0f));

    pushChord (proc, *editor, 5, 9, { 53, 60, 65, 72, 77 });         // F DRONE
    pushChord (proc, *editor, 0, 6, { 48, 55, 60, 67, 72 });         // C POWER
    pushChord (proc, *editor, 2, 0, { 50, 57, 62, 69, 74 });         // D TRIAD
    pushChord (proc, *editor, 9, 1, { 57, 60, 64, 67, 69 });         // A 7TH, newest

    writePng (*editor, outDir.getChildFile ("panel.png"));

    /* Second state: hand-set root, fixed chord style, NOTE-follow with TRACK
       on would dim MODE, so show that separately. */
    setValue (proc, pid::track, 0.0f);
    setValue (proc, pid::random, 0.0f);
    setValue (proc, pid::root, 4.0f);                   // E
    setValue (proc, pid::chord, 6.0f);                  // POWER
    setValue (proc, pid::scale, 6.0f);                  // HARM MINOR
    setValue (proc, pid::voices, 1.0f);                 // three voices
    setValue (proc, pid::range, 2.0f);
    live.effectiveRootPc.store (4);
    live.limGrDb.store (3.0f);                          // limiter working
    live.gateOpen.store (false);
    live.detectedMidi.store (-1);
    live.outLevel.store (0.9f);
    editor->pollLive();

    writePng (*editor, outDir.getChildFile ("panel_manual.png"));

    /* Third: TRACK on with FOLLOWS = NOTE, so MODE is dimmed. */
    setValue (proc, pid::track, 1.0f);
    setValue (proc, pid::follows, 0.0f);
    setValue (proc, pid::random, 1.0f);
    live.effectiveRootPc.store (9);
    live.detectedMidi.store (69);
    live.detectedCents.store (-12.0f);
    editor->pollLive();

    writePng (*editor, outDir.getChildFile ("panel_note.png"));

    return 0;
}
