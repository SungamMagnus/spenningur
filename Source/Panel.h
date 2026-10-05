#pragma once

#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

/**
 * Flat drawing primitives and the panel grid for SPENNINGUR, in a fixed
 * 540 x 670 design space. The editor applies one scale transform, so every
 * layout constant here doubles as hit-test geometry.
 *
 * The numbers are the ones in docs/mockup/index.html, measured off the
 * rendered page (the mockup is the spec): one vertical column of sections,
 * 12 px apart, signal order top to bottom.
 */
namespace spn::panel
{

constexpr float designW = 540.0f, designH = 670.0f;

/* ── Palette ─────────────────────────────────────────────────────────────
 * Paper and ink are the constant; every accent is a section colour, never
 * decoration. See docs/mockup/ds/tokens/colors.css. */
namespace hue
{
const juce::Colour paper  { 0xfff0ece2 };
const juce::Colour ink    { 0xff1a1a17 };
const juce::Colour coral  { 0xffed8159 };   // the engine that decides the chord
const juce::Colour lilac  { 0xffd3a7dc };   // pale trims
const juce::Colour teal   { 0xff52b0a4 };   // the resonator
const juce::Colour steel  { 0xff4f7ea8 };   // everything after the sections are summed
const juce::Colour violet { 0xff6b5bc4 };   // modulation, and only modulation
const juce::Colour amber  { 0xffc08d16 };   // the limiter
}

inline juce::Colour ink (float alpha) { return hue::ink.withAlpha (alpha); }

/** A readable relative of a hue too pale to write small type in. */
juce::Colour textHue (juce::Colour);

/* ── Sections (outer rectangles, design space) ───────────────────────────── */
inline juce::Rectangle<float> frameInput()       { return { 21.0f,  25.0f, 168.0f, 172.0f }; }
inline juce::Rectangle<float> frameHarmony()     { return { 201.0f, 25.0f, 320.0f, 172.0f }; }
inline juce::Rectangle<float> frameProgression() { return { 21.0f, 209.0f, 500.0f, 158.0f }; }
inline juce::Rectangle<float> frameResonator()   { return { 21.0f, 379.0f, 376.0f, 248.0f }; }
inline juce::Rectangle<float> frameOutput()      { return { 409.0f, 379.0f, 112.0f, 248.0f }; }

/* ── Knobs ───────────────────────────────────────────────────────────────
 * A knob cell is anchored on its centre x and the top of its label; the dial
 * sits 42 px + r below that, which is the mockup's label block (26) plus its
 * margins, overlapped by the dial's own padding. Radius says importance. */
constexpr float rXl = 38.0f, rLg = 26.0f, rMd = 20.0f, rSm = 15.0f;

inline float knobCy (float labelTop, float r) { return labelTop + r + 42.0f; }

/** The drag target of a knob cell: label, value and dial. */
inline juce::Rectangle<float> knobHit (float cx, float labelTop, float r)
{
    const float cy = knobCy (labelTop, r);
    const float half = juce::jmax (r + 9.0f, 28.0f);
    return { cx - half, labelTop - 2.0f, half * 2.0f, (cy + r + 9.0f) - (labelTop - 2.0f) };
}

/* ── Faders ──────────────────────────────────────────────────────────────── */
constexpr float faderW = 44.0f, faderTravel = 72.0f, faderTrackTop = 24.0f;

/** The slider column: label above, 120 px of track, value below. */
inline juce::Rectangle<float> faderHit (float x, float labelTop)
{
    return { x, labelTop - 2.0f, faderW, 16.0f + 120.0f + 6.0f + 11.0f + 4.0f };
}
inline float faderSvgTop (float labelTop) { return labelTop + 16.0f; }

/* ── Type ────────────────────────────────────────────────────────────────── */
juce::Font mono (float h, bool bold = false);

/** Width of a run of text in the mono face, plus CSS-style tracking (in em). */
float textWidth (const juce::String&, float size, bool bold = false, float trackingEm = 0.0f);

void text (juce::Graphics&, const juce::String&, juce::Rectangle<float>, float size, juce::Colour,
           juce::Justification = juce::Justification::centred, bool bold = false);

/** Letter-spaced run: the wordmark. */
void tracked (juce::Graphics&, const juce::String&, juce::Rectangle<float>, float size, juce::Colour,
              float trackingPx, bool leftAlign = true, bool bold = true);

/* ── Furniture ───────────────────────────────────────────────────────────── */

/** A section: a hairline box in the section colour, its title sitting in the rule. */
void frame (juce::Graphics&, juce::Rectangle<float>, const juce::String& title, juce::Colour);

void wordmark (juce::Graphics&, float x, float y, const juce::String&, float size = 16.0f);

void lamp (juce::Graphics&, juce::Rectangle<float> square, bool on, juce::Colour);

/** A readout: hairline box in a colour, text left-aligned inside. */
void valueBox (juce::Graphics&, juce::Rectangle<float>, const juce::String&, juce::Colour,
               bool filled = false, float alpha = 1.0f, float size = 9.5f,
               juce::Justification = juce::Justification::centredLeft);

/** A small label above a control. */
void label (juce::Graphics&, juce::Rectangle<float>, const juce::String&);

/* ── Controls ────────────────────────────────────────────────────────────── */

/** One circle, one arc, one pointer. The arc grows from the CCW stop. */
void knob (juce::Graphics&, float cx, float cy, float r, float norm, juce::Colour);

/** The dial with designation and value above it. */
void knobCell (juce::Graphics&, float cx, float labelTop, float r, float norm, juce::Colour,
               const juce::String& label, const juce::String& value);

void faderCell (juce::Graphics&, float x, float labelTop, float norm, juce::Colour,
                const juce::String& label, const juce::String& value);

void latch (juce::Graphics&, juce::Rectangle<float>, bool on, juce::Colour,
            const juce::String& label, float size = 8.0f);

/** N-position selector; every position labelled in full. */
struct SelectorLayout
{
    juce::Rectangle<float> outer;
    std::vector<juce::Rectangle<float>> seg;
    float size = 8.5f;
    bool vertical = false;

    int indexAt (juce::Point<float>) const;
    SelectorLayout translated (float dx, float dy = 0.0f) const;
};
SelectorLayout selectorLayout (const juce::StringArray& options, float size, bool vertical, float x, float y);
void selector (juce::Graphics&, const SelectorLayout&, const juce::StringArray& options, int selected,
               juce::Colour);

/** The dropdown: an outlined box carrying the value and a small square that is filled while open. */
void dropdown (juce::Graphics&, juce::Rectangle<float>, const juce::String& value, juce::Colour, bool open);

/** The envelope trace in its hairline box, with its TRANSIENT / BLOOM / PAD word. */
void envPlot (juce::Graphics&, juce::Rectangle<float>, float a, float d, float s, float r, juce::Colour);

void meter (juce::Graphics&, juce::Rectangle<float>, float value, juce::Colour);

/** A level meter stood on end, filling from the bottom; the top segment is always amber. */
void segMeter (juce::Graphics&, float x, float top, float level, juce::Colour, int segments = 6);

/* ── The dropdown menu ───────────────────────────────────────────────────── */

/** Flat paper / ink / mono styling for the dropdown's popup list. */
class MenuLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    MenuLookAndFeel();

    float scale = 1.0f;                  // editor scale at the time the menu opens
    juce::Colour accent = hue::coral;    // the dropdown's colour

    void drawPopupMenuBackgroundWithOptions (juce::Graphics&, int w, int h, const juce::PopupMenu::Options&) override;
    void drawPopupMenuItemWithOptions (juce::Graphics&, const juce::Rectangle<int>&, bool isHighlighted,
                                       const juce::PopupMenu::Item&, const juce::PopupMenu::Options&) override;
    void getIdealPopupMenuItemSizeWithOptions (const juce::String&, bool isSeparator, int standardHeight,
                                               int& idealWidth, int& idealHeight, const juce::PopupMenu::Options&) override;
    int getPopupMenuBorderSizeWithOptions (const juce::PopupMenu::Options&) override { return 1; }
    int getMenuWindowFlags() override { return 0; }     // flat: no drop shadow
    juce::Font getPopupMenuFont() override;
};

} // namespace spn::panel
