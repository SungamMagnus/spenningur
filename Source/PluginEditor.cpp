#include "PluginEditor.h"

#include <cmath>

#include "ParamIDs.h"
#include "spenningur/Harmony.h"

using namespace spn::panel;

namespace
{
/* The CHORD dropdown, the ROOT dropdown and the SCALE dropdown are all one
   widget; these are their positions (design space). */
constexpr float kDropY1 = 62.5f, kDropY2 = 147.0f, kDropH = 21.0f;

float dbToMeter (float lin)
{
    if (lin <= 1.0e-5f)
        return 0.0f;
    return juce::jlimit (0.0f, 1.0f, (20.0f * std::log10 (lin) + 48.0f) / 48.0f);
}

juce::String oneDecimal (float v)
{
    if (std::abs (v) < 0.05f)
        v = 0.0f;
    return juce::String (v, 1);
}

juce::String msText (float ms)
{
    return ms < 1000.0f ? juce::String (juce::roundToInt (ms)) + " ms" : oneDecimal (ms / 1000.0f) + " s";
}

juce::String chordText (int rootPc, int style)
{
    return juce::String (spn::rootName (rootPc)) + " " + spn::styleName ((spn::ChordStyle) style);
}
} // namespace

SpenningurEditor::SpenningurEditor (SpenningurProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setOpaque (true);
    setLookAndFeel (&menuLnf);
    buildControls();

    setResizable (true, true);
    setResizeLimits (400, (int) (400.0f * designH / designW), 1600, (int) (1600.0f * designH / designW));
    getConstrainer()->setFixedAspectRatio ((double) designW / (double) designH);
    setSize ((int) designW, (int) designH);

    startTimerHz (30);
}

SpenningurEditor::~SpenningurEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

/* ── Control table ───────────────────────────────────────────────────────── */

juce::RangedAudioParameter* SpenningurEditor::param (const char* id) const
{
    auto* p = proc.apvts.getParameter (id);
    jassert (p != nullptr);
    return p;
}

void SpenningurEditor::buildControls()
{
    namespace pid = spn::pid;

    auto addKnob = [this] (const char* id, float cx, float top, float r, juce::Colour c, const char* label, Fmt fmt)
    {
        KnobCtl k;
        k.param = param (id);
        k.cx = cx; k.top = top; k.r = r; k.colour = c; k.label = label; k.fmt = fmt;
        k.travel = r >= 30.0f ? 100.0f : 80.0f;
        k.hit = knobHit (cx, top, r);
        knobs.push_back (k);
    };

    // INPUT
    addKnob (pid::in,   63.0f,  55.5f, rSm, hue::coral, "IN",   Fmt::inDb);
    addKnob (pid::gate, 121.0f, 55.5f, rSm, hue::lilac, "GATE", Fmt::gateDb);
    // PROGRESSION
    addKnob (pid::tension, 147.0f, 238.5f, rLg, hue::coral, "TENSION", Fmt::percent);
    addKnob (pid::hold,    226.0f, 239.5f, rSm, hue::lilac, "HOLD",    Fmt::ms);
    // RESONATOR
    addKnob (pid::ring,   76.0f,  408.5f, rLg, hue::teal,  "RING",   Fmt::seconds);
    addKnob (pid::damp,   156.0f, 408.5f, rMd, hue::teal,  "DAMP",   Fmt::kHz);
    addKnob (pid::spread, 76.0f,  529.0f, rMd, hue::teal,  "SPREAD", Fmt::oct);
    addKnob (pid::glide,  156.0f, 530.0f, rSm, hue::lilac, "GLIDE",  Fmt::ms);
    // OUTPUT
    addKnob (pid::mix, 465.0f, 412.5f, rLg, hue::steel, "MIX", Fmt::wet);

    auto addFader = [this] (const char* id, float x, const char* label, Fmt fmt)
    {
        FaderCtl f;
        f.param = param (id);
        f.x = x; f.label = label; f.fmt = fmt;
        f.hit = faderHit (x, f.top);
        faders.push_back (f);
    };
    addFader (pid::atk, 198.0f, "ATTACK",  Fmt::envMs);
    addFader (pid::dec, 244.0f, "DECAY",   Fmt::envMs);
    addFader (pid::sus, 290.0f, "SUSTAIN", Fmt::percent);
    addFader (pid::rel, 336.0f, "RELEASE", Fmt::envMs);

    auto addLatch = [this] (const char* id, juce::Rectangle<float> r, const char* label, juce::Colour c)
    {
        LatchCtl l;
        l.param = param (id);
        l.rect = r; l.label = label; l.colour = c;
        latches.push_back (l);
    };
    addLatch (pid::track,  { 282.0f, 64.5f, 50.0f, 19.0f }, "TRACK",  hue::violet);   // lTrack
    addLatch (pid::random, { 310.0f, 149.0f, 58.0f, 19.0f }, "RANDOM", hue::violet);   // lRandom
    addLatch (pid::lim,    { 426.0f, 548.5f, 46.0f, 19.0f }, "LIM",    hue::amber);    // lLim

    auto addSel = [this] (const char* id, juce::StringArray opts, bool vertical, juce::Colour c, float x, float y)
    {
        SelectorCtl s;
        s.param = param (id);
        s.options = std::move (opts);
        s.vertical = vertical;
        s.colour = c;
        s.layout = selectorLayout (s.options, 8.5f, vertical, x, y);
        selectors.push_back (std::move (s));
    };
    addSel (pid::range,   { "SMALL", "MEDIUM", "LARGE" },      false, hue::coral,  38.0f, 158.0f);        // sRange
    addSel (pid::follows, { "NOTE", "KEY" },                    false, hue::violet, 340.0f, 61.5f);       // sFollows
    addSel (pid::voices,  { "2", "3", "4", "5", "6" },          false, hue::coral,  395.0f, 146.0f); // sVoices
    addSel (pid::mode,    { "FOLLOW", "CADENCE", "WANDER" },    true,  hue::coral,  38.0f,  245.5f);       // sMode

    juce::StringArray notes, scales, styles;
    for (int i = 0; i < 12; ++i) notes.add (spn::rootName (i));
    for (int i = 0; i < spn::kNumScales; ++i) scales.add (spn::scaleDef (i).name);
    for (int i = 0; i < spn::kNumStyles; ++i) styles.add (spn::styleName ((spn::ChordStyle) i));

    drops[dRoot]  = { param (pid::root),  notes,  { 218.0f, kDropY1, 56.0f,  kDropH } };
    drops[dScale] = { param (pid::scale), scales, { 404.0f, kDropY1, 96.0f,  kDropH } };
    drops[dChord] = { param (pid::chord), styles, { 218.0f, kDropY2, 84.0f,  kDropH } };
}

float SpenningurEditor::scale() const { return (float) getWidth() / designW; }

juce::Point<float> SpenningurEditor::toDesign (juce::Point<float> px) const
{
    const float k = juce::jmax (0.0001f, scale());
    return { px.x / k, px.y / k };
}

/* ── Parameter helpers ───────────────────────────────────────────────────── */

int SpenningurEditor::choiceIndex (const juce::RangedAudioParameter& p) const
{
    return juce::roundToInt (p.convertFrom0to1 (p.getValue()));
}

bool SpenningurEditor::boolValue (const juce::RangedAudioParameter& p) const { return p.getValue() > 0.5f; }

void SpenningurEditor::setChoice (juce::RangedAudioParameter& p, int index)
{
    p.beginChangeGesture();
    p.setValueNotifyingHost (p.convertTo0to1 ((float) index));
    p.endChangeGesture();
}

void SpenningurEditor::toggle (juce::RangedAudioParameter& p)
{
    p.beginChangeGesture();
    p.setValueNotifyingHost (boolValue (p) ? 0.0f : 1.0f);
    p.endChangeGesture();
}

juce::String SpenningurEditor::format (Fmt f, float v) const
{
    switch (f)
    {
        case Fmt::inDb:    return oneDecimal (v) + " dB";
        case Fmt::gateDb:  return juce::String (juce::roundToInt (v)) + " dB";
        case Fmt::percent: return juce::String (juce::roundToInt (v * 100.0f)) + "%";
        case Fmt::ms:      return juce::String (juce::roundToInt (v)) + " ms";
        case Fmt::seconds: return oneDecimal (v) + " s";
        case Fmt::kHz:     return oneDecimal (v / 1000.0f) + " kHz";
        case Fmt::oct:     return oneDecimal (v) + " oct";
        case Fmt::wet:     return juce::String (juce::roundToInt (v * 100.0f)) + "% wet";
        case Fmt::envMs:   return msText (v);
    }
    return {};
}

juce::String SpenningurEditor::readout (const KnobCtl& k) const
{
    return format (k.fmt, k.param->convertFrom0to1 (k.param->getValue()));
}

juce::String SpenningurEditor::readout (const FaderCtl& f) const
{
    return format (f.fmt, f.param->convertFrom0to1 (f.param->getValue()));
}

/* ── Live state ──────────────────────────────────────────────────────────── */

void SpenningurEditor::pollLive()
{
    auto& live = proc.live;

    const uint32_t serial = live.chordSerial.load (std::memory_order_acquire);
    if (serial != lastSerial)
    {
        lastSerial = serial;
        const int n = juce::jlimit (0, 6, live.chordCount.load (std::memory_order_relaxed));
        if (n > 0)
        {
            ChordRec c;
            c.rootPc = live.chordRootPc.load (std::memory_order_relaxed);
            c.style  = live.chordStyle.load (std::memory_order_relaxed);
            c.count  = n;
            for (int i = 0; i < n; ++i)
                c.midi[i] = live.chordMidi[i].load (std::memory_order_relaxed);

            history.push_back (c);
            if (history.size() > 4)
                history.erase (history.begin());
        }
    }

    /* Peak meters: instant attack, a quick fall, so a transient is still readable. */
    inDisp  = juce::jmax (dbToMeter (live.inLevel.load (std::memory_order_relaxed)),  inDisp * 0.82f);
    outDisp = juce::jmax (dbToMeter (live.outLevel.load (std::memory_order_relaxed)), outDisp * 0.82f);
}

/* ── Interaction ─────────────────────────────────────────────────────────── */

void SpenningurEditor::openDropdown (int which)
{
    auto& d = drops[which];
    const bool track = boolValue (*param (spn::pid::track));
    const bool random = boolValue (*param (spn::pid::random));

    if (which == dRoot && track)
        return;                                   // locked: shows the live root, ignores clicks

    int shown = choiceIndex (*d.param);
    juce::Colour colour = hue::coral;
    if (which == dRoot && track)   { shown = proc.live.effectiveRootPc.load(); colour = hue::violet; }
    if (which == dChord && random) { shown = proc.live.chordStyle.load();      colour = hue::violet; }

    menuLnf.scale = scale();
    menuLnf.accent = colour;

    juce::PopupMenu menu;
    menu.setLookAndFeel (&menuLnf);
    for (int i = 0; i < d.options.size(); ++i)
        menu.addItem (i + 1, d.options[i], true, i == shown);

    const auto area = localAreaToGlobal (juce::Rectangle<float> (d.rect.getX() * scale(), d.rect.getY() * scale(),
                                                                 d.rect.getWidth() * scale(), d.rect.getHeight() * scale())
                                             .getSmallestIntegerContainer());
    openDrop = which;
    repaint();

    juce::Component::SafePointer<SpenningurEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withTargetComponent (this)
                            .withTargetScreenArea (area)
                            .withMinimumWidth (area.getWidth())
                            .withPreferredPopupDirection (juce::PopupMenu::Options::PopupDirection::downwards),
                        [safe, which] (int result)
                        {
                            if (safe == nullptr)
                                return;
                            safe->openDrop = -1;
                            if (result > 0)
                                safe->setChoice (*safe->drops[which].param, result - 1);
                            safe->repaint();
                        });
}

void SpenningurEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto d = toDesign (e.position);

    for (auto& l : latches)
        if (l.rect.expanded (2.0f).contains (d))
            return toggle (*l.param);

    for (auto& s : selectors)
    {
        const int i = s.layout.indexAt (d);
        if (i >= 0)
        {
            setChoice (*s.param, i);
            repaint();
            return;
        }
    }

    for (int i = 0; i < numDrops; ++i)
        if (drops[i].rect.contains (d))
            return openDropdown (i);

    for (size_t i = 0; i < faders.size(); ++i)
        if (faders[i].hit.contains (d))
        {
            dragKind = Drag::fader;
            dragIdx = (int) i;
            dragTravel = faderTravel;
            dragStartNorm = faders[i].param->getValue();
            dragStart = d;
            faders[i].param->beginChangeGesture();
            setMouseCursor (juce::MouseCursor::NoCursor);
            return;
        }

    for (size_t i = 0; i < knobs.size(); ++i)
        if (knobs[i].hit.contains (d))
        {
            dragKind = Drag::knob;
            dragIdx = (int) i;
            dragTravel = knobs[i].travel;
            dragStartNorm = knobs[i].param->getValue();
            dragStart = d;
            knobs[i].param->beginChangeGesture();
            setMouseCursor (juce::MouseCursor::NoCursor);
            return;
        }
}

void SpenningurEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragKind == Drag::none)
        return;

    auto* p = dragKind == Drag::knob ? knobs[(size_t) dragIdx].param : faders[(size_t) dragIdx].param;
    const auto d = toDesign (e.position);
    const float sensitivity = e.mods.isShiftDown() ? 0.22f : 1.0f;
    const float delta = (dragStart.y - d.y) / dragTravel;

    p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, dragStartNorm + delta * sensitivity));
    repaint();
}

void SpenningurEditor::mouseUp (const juce::MouseEvent&)
{
    if (dragKind == Drag::none)
        return;

    auto* p = dragKind == Drag::knob ? knobs[(size_t) dragIdx].param : faders[(size_t) dragIdx].param;
    p->endChangeGesture();
    dragKind = Drag::none;
    dragIdx = -1;
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

void SpenningurEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto d = toDesign (e.position);
    juce::RangedAudioParameter* target = nullptr;

    for (auto& f : faders)
        if (f.hit.contains (d)) target = f.param;
    for (auto& k : knobs)
        if (k.hit.contains (d)) target = k.param;

    if (target == nullptr)
        return;

    target->beginChangeGesture();
    target->setValueNotifyingHost (target->getDefaultValue());
    target->endChangeGesture();
    repaint();
}

void SpenningurEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const auto d = toDesign (e.position);
    juce::RangedAudioParameter* target = nullptr;

    for (auto& f : faders)
        if (f.hit.contains (d)) target = f.param;
    for (auto& k : knobs)
        if (k.hit.contains (d)) target = k.param;

    if (target == nullptr)
        return;

    const float gain = e.mods.isShiftDown() ? 0.04f : 0.16f;
    const float delta = w.deltaY * (w.isReversed ? -1.0f : 1.0f) * gain;

    target->beginChangeGesture();
    target->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, target->getValue() + delta));
    target->endChangeGesture();
    repaint();
}

/* ── Paint ───────────────────────────────────────────────────────────────── */

void SpenningurEditor::paint (juce::Graphics& g)
{
    g.fillAll (hue::paper);
    g.addTransform (juce::AffineTransform::scale (scale()));

    auto& live = proc.live;
    namespace pid = spn::pid;

    const bool track  = boolValue (*param (pid::track));
    const bool random = boolValue (*param (pid::random));
    const bool follows_note = choiceIndex (*param (pid::follows)) == 0;
    const int  scaleIdx = choiceIndex (*param (pid::scale));

    const int effRoot = juce::jlimit (0, 11, live.effectiveRootPc.load (std::memory_order_relaxed));
    const int liveStyle = juce::jlimit (0, spn::kNumStyles - 1, live.chordStyle.load (std::memory_order_relaxed));

    const auto rootColour  = track ? hue::violet : hue::coral;
    const auto styleColour = random ? hue::violet : hue::coral;

    g.setColour (ink (0.22f));
    g.drawRect (juce::Rectangle<float> (0.0f, 0.0f, designW, designH), 1.0f);

    /* ── Sections ────────────────────────────────────────────────────────── */
    frame (g, frameInput(),       "INPUT",       hue::coral);
    frame (g, frameHarmony(),     "HARMONY",     hue::coral);
    frame (g, frameProgression(), "PROGRESSION", hue::coral);
    frame (g, frameResonator(),   "RESONATOR",   hue::teal);
    frame (g, frameOutput(),      "OUTPUT",      hue::steel);

    /* ── Knobs and faders ────────────────────────────────────────────────── */
    for (auto& k : knobs)
        knobCell (g, k.cx, k.top, k.r, k.param->getValue(), k.colour, k.label, readout (k));

    for (auto& f : faders)
        faderCell (g, f.x, f.top, f.param->getValue(), hue::teal, f.label, readout (f));

    /* ── INPUT ───────────────────────────────────────────────────────────── */
    const auto& range = selectors[sRange];
    label (g, { 38.0f, 144.5f, 134.0f, 9.5f }, "RANGE");
    selector (g, range.layout, range.options, choiceIndex (*range.param), range.colour);

    lamp (g, { 159.5f, 61.0f, 9.0f, 9.0f }, live.gateOpen.load (std::memory_order_relaxed), hue::coral);
    segMeter (g, 156.0f, 77.0f, inDisp, hue::coral, 6);

    /* ── HARMONY ─────────────────────────────────────────────────────────── */
    label (g, { 218.0f, 49.0f, 56.0f, 9.5f }, "ROOT");
    dropdown (g, drops[dRoot].rect, drops[dRoot].options[track ? effRoot : choiceIndex (*drops[dRoot].param)],
              rootColour, openDrop == dRoot);

    latch (g, latches[lTrack].rect, track, hue::violet, "TRACK");

    const auto& fol = selectors[sFollows];
    label (g, { fol.layout.outer.getX(), 48.0f, fol.layout.outer.getWidth(), 9.5f }, "FOLLOWS");
    selector (g, fol.layout, fol.options, choiceIndex (*fol.param), fol.colour);

    label (g, { 404.0f, 49.0f, 96.0f, 9.5f }, "SCALE");
    dropdown (g, drops[dScale].rect, drops[dScale].options[scaleIdx], hue::coral, openDrop == dScale);

    g.setColour (ink (0.13f));
    g.fillRect (juce::Rectangle<float> (218.0f, 107.5f, 286.0f, 1.0f));

    label (g, { 218.0f, 133.5f, 84.0f, 9.5f }, "CHORD");
    dropdown (g, drops[dChord].rect, drops[dChord].options[random ? liveStyle : choiceIndex (*drops[dChord].param)],
              styleColour, openDrop == dChord);

    latch (g, latches[lRandom].rect, random, hue::violet, "RANDOM");
    lamp (g, { 377.0f, 149.0f, 9.0f, 9.0f }, random, hue::violet);

    const auto& vox = selectors[sVoices];
    label (g, { vox.layout.outer.getX(), 132.5f, vox.layout.outer.getWidth(), 9.5f }, "VOICES");
    selector (g, vox.layout, vox.options, choiceIndex (*vox.param), vox.colour);

    /* ── PROGRESSION ─────────────────────────────────────────────────────── */
    {
        /* With TRACK on and FOLLOWS = NOTE the chord root is always degree 0,
           so MODE has nothing to choose: the panel dims it. */
        const bool bypassed = track && follows_note;
        const auto& mode = selectors[sMode];

        if (bypassed)
            g.beginTransparencyLayer (0.38f);
        label (g, { 38.0f, 232.0f, 55.0f, 9.5f }, "MODE");
        selector (g, mode.layout, mode.options, choiceIndex (*mode.param), mode.colour);
        if (bypassed)
            g.endTransparencyLayer();
    }

    {
        /* The two content-width boxes of the mockup are held at one width so
           the layout does not jitter as the readouts change. */
        constexpr float colX = 269.0f, colR = 504.0f, leftW = 82.0f;
        const float restX = colX + leftW + 8.0f, restW = colR - restX;

        const int m = live.detectedMidi.load (std::memory_order_relaxed);
        juce::String det = "-";
        if (m >= 0)
        {
            const int cents = juce::roundToInt (live.detectedCents.load (std::memory_order_relaxed));
            det = juce::String (spn::rootName (m % 12)) + juce::String (m / 12 - 1) + "  "
                  + (cents >= 0 ? "+" : "-") + juce::String (std::abs (cents)) + " ct";
        }
        label (g, { colX, 232.0f, leftW, 9.5f }, "DETECTED");
        valueBox (g, { colX, 245.5f, leftW, 21.0f }, det, hue::coral);

        const auto rootText = juce::String (spn::rootName (effRoot)) + " " + spn::scaleDef (scaleIdx).name;
        label (g, { restX, 232.0f, restW, 9.5f }, "ROOT");
        valueBox (g, { restX, 245.5f, restW, 21.0f }, rootText, rootColour);

        const bool have = ! history.empty();
        const ChordRec cur = have ? history.back() : ChordRec();

        label (g, { colX, 272.5f, leftW, 9.5f }, "CHORD");
        valueBox (g, { colX, 286.0f, leftW, 21.0f },
                  have ? chordText (cur.rootPc, cur.style) : juce::String ("-"), hue::coral);

        label (g, { restX, 272.5f, restW, 9.5f }, "TONES");
        juce::String tones;
        if (have)
            for (int i = 0; i < cur.count; ++i)
                tones << (i ? " " : "") << spn::rootName (cur.midi[i] % 12);
        valueBox (g, { restX, 286.0f, restW, 21.0f }, have ? tones : juce::String ("-"), hue::coral);

        label (g, { colX, 313.0f, 100.0f, 9.5f }, "HISTORY");
        for (int i = 0; i < 4; ++i)
        {
            const int hi = (int) history.size() - 4 + i;
            const bool filled = hi >= 0;
            valueBox (g, { colX + (float) i * 59.5f, 326.5f, 56.5f, 18.0f },
                      filled ? chordText (history[(size_t) hi].rootPc, history[(size_t) hi].style) : juce::String ("-"),
                      hue::coral, filled && i == 3, filled ? 1.0f : 0.35f, 7.0f, juce::Justification::centred);
        }
    }

    /* ── RESONATOR ───────────────────────────────────────────────────────── */
    envPlot (g, { 198.0f, 402.0f, 182.0f, 46.0f }, faders[0].param->getValue(), faders[1].param->getValue(),
             faders[2].param->getValue(), faders[3].param->getValue(), hue::teal);

    /* ── OUTPUT ──────────────────────────────────────────────────────────── */
    latch (g, latches[lLim].rect, boolValue (*latches[lLim].param), hue::amber, "LIM");
    lamp (g, { 483.0f, 553.5f, 9.0f, 9.0f }, live.limGrDb.load (std::memory_order_relaxed) > 0.1f, hue::amber);

    label (g, { 426.0f, 577.5f, 78.0f, 9.5f }, "LEVEL");
    meter (g, { 426.0f, 597.0f, 80.0f, 9.0f }, outDisp, hue::steel);

    wordmark (g, 21.0f, 638.0f, "SPENNINGUR");
}
