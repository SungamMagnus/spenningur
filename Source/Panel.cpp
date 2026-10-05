#include "Panel.h"

namespace spn::panel
{

namespace
{
/* The house pot sweep: 317.2 degrees, symmetric about noon, leaving the gap at
   the bottom where the pointer never goes. Angles are juce's: radians
   clockwise from 12 o'clock. */
constexpr float kSweep = 158.6f * juce::MathConstants<float>::pi / 180.0f;

float angleFor (float norm) { return -kSweep + 2.0f * kSweep * juce::jlimit (0.0f, 1.0f, norm); }

/** CSS "line-height: normal" for the mono face, to the half pixel. */
float lineHeight (float fs) { return std::round (fs * 1.17f * 2.0f) / 2.0f; }
} // namespace

juce::Colour textHue (juce::Colour c)
{
    if (c == hue::lilac) return juce::Colour (0xff9a63ab);
    return c;
}

juce::Font mono (float h, bool bold)
{
    return juce::Font (juce::FontOptions()
                           .withName (juce::Font::getDefaultMonospacedFontName())
                           .withHeight (h)
                           .withStyle (bold ? "Bold" : "Regular"));
}

float textWidth (const juce::String& s, float size, bool bold, float trackingEm)
{
    return juce::GlyphArrangement::getStringWidth (mono (size, bold), s)
           + trackingEm * size * (float) s.length();
}

void text (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r, float size,
           juce::Colour c, juce::Justification j, bool bold)
{
    g.setColour (c);
    g.setFont (mono (size, bold));
    g.drawText (s, r, j, false);
}

void tracked (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r, float size,
              juce::Colour c, float tracking, bool leftAlign, bool bold)
{
    g.setColour (c);
    g.setFont (mono (size, bold));
    const auto f = g.getCurrentFont();

    float total = -tracking;
    for (int i = 0; i < s.length(); ++i)
        total += juce::GlyphArrangement::getStringWidth (f, s.substring (i, i + 1)) + tracking;

    float x = leftAlign ? r.getX() : r.getCentreX() - total * 0.5f;
    for (int i = 0; i < s.length(); ++i)
    {
        const auto ch = s.substring (i, i + 1);
        const float w = juce::GlyphArrangement::getStringWidth (f, ch);
        g.drawText (ch, juce::Rectangle<float> (x, r.getY(), w, r.getHeight()),
                    juce::Justification::centred, false);
        x += w + tracking;
    }
}

/* ── Furniture ───────────────────────────────────────────────────────────── */

void frame (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title, juce::Colour c)
{
    g.setColour (c);
    g.drawRect (r, 1.0f);

    if (title.isEmpty())
        return;

    /* The title sits in the rule: a paper patch with the text on it. */
    const float w = textWidth (title, 8.5f, false, 0.04f) + 12.0f;
    const juce::Rectangle<float> patch (r.getX() + 15.0f, r.getY() - 7.0f, w, 10.0f);
    g.setColour (hue::paper);
    g.fillRect (patch);
    text (g, title, patch, 8.5f, c);
}

void wordmark (juce::Graphics& g, float x, float y, const juce::String& s, float size)
{
    tracked (g, s, { x, y, 200.0f, lineHeight (size) + 1.0f }, size, ink (0.55f), 0.28f * size, true, true);
}

void lamp (juce::Graphics& g, juce::Rectangle<float> sq, bool on, juce::Colour c)
{
    if (on)
    {
        g.setColour (c);
        g.fillRect (sq);
    }
    else
    {
        g.setColour (hue::paper);
        g.fillRect (sq);
        g.setColour (c.withAlpha (0.45f));
        g.drawRect (sq.expanded (0.5f), 1.0f);     // the stroke is centred on the edge
    }
}

void valueBox (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& s, juce::Colour c,
               bool filled, float alpha, float size, juce::Justification j)
{
    c = c.withMultipliedAlpha (alpha);

    if (filled)
    {
        g.setColour (c);
        g.fillRect (r);
    }
    g.setColour (c);
    g.drawRect (r, 1.0f);
    text (g, s, r.reduced (9.0f, 0.0f), size, filled ? hue::paper.withMultipliedAlpha (alpha) : c, j);
}

void label (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& s)
{
    text (g, s, r, 8.0f, ink (0.62f), juce::Justification::centredLeft);
}

/* ── Controls ────────────────────────────────────────────────────────────── */

void knob (juce::Graphics& g, float cx, float cy, float r, float norm, juce::Colour colour)
{
    const juce::Point<float> c (cx, cy);
    norm = juce::jlimit (0.0f, 1.0f, norm);

    const float track = r + 4.5f;
    const float w = r >= 24.0f ? 3.4f : (r >= 16.0f ? 2.8f : 2.2f);
    const float a0 = angleFor (0.0f), a1 = angleFor (1.0f), aNow = angleFor (norm);

    g.setColour (hue::paper);
    g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
    g.setColour (ink (0.38f));
    g.drawEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c), 1.2f);

    auto arc = [&] (float from, float to, juce::Colour col)
    {
        if (std::abs (to - from) < 1.0e-4f)
            return;
        juce::Path p;
        p.addCentredArc (cx, cy, track, track, 0.0f, from, to, true);
        g.setColour (col);
        g.strokePath (p, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    };
    arc (a0, a1, ink (0.13f));
    arc (a0, aNow, colour);

    g.setColour (ink (0.85f));
    g.drawLine ({ c.getPointOnCircumference (r * 0.16f, aNow), c.getPointOnCircumference (r * 0.78f, aNow) },
                r >= 24.0f ? 2.0f : 1.5f);

    g.setColour (ink (0.28f));
    g.drawLine ({ c.getPointOnCircumference (r + 1.5f, a0), c.getPointOnCircumference (r + 6.5f, a0) }, 1.0f);
}

void knobCell (juce::Graphics& g, float cx, float labelTop, float r, float norm, juce::Colour colour,
               const juce::String& lab, const juce::String& value)
{
    const float fs = r >= 30.0f ? 10.0f : (r >= 18.0f ? 9.0f : 8.0f);
    const float lh = lineHeight (fs), vh = lineHeight (fs + 2.0f);

    if (lab.isNotEmpty())
        text (g, lab, { cx - 45.0f, labelTop, 90.0f, lh }, fs, ink (0.62f));
    if (value.isNotEmpty())
        text (g, value, { cx - 45.0f, labelTop + lh, 90.0f, vh }, fs + 2.0f, textHue (colour),
              juce::Justification::centred, true);

    knob (g, cx, knobCy (labelTop, r), r, norm, colour);
}

void faderCell (juce::Graphics& g, float x, float labelTop, float norm, juce::Colour colour,
                const juce::String& lab, const juce::String& value)
{
    const float top = faderSvgTop (labelTop) + faderTrackTop;
    const float bot = top + faderTravel;
    const float cx = x + faderW * 0.5f;
    norm = juce::jlimit (0.0f, 1.0f, norm);
    const float y = bot - norm * faderTravel;

    text (g, lab, { x - 4.0f, labelTop, faderW + 8.0f, 10.0f }, 8.5f, ink (0.62f));

    for (int i = 0; i < 9; ++i)
    {
        const float ty = top + faderTravel * (float) i / 8.0f;
        g.setColour (ink (0.13f));
        g.fillRect (juce::Rectangle<float> (cx - 15.0f, ty - 0.5f, i % 4 == 0 ? 6.0f : 3.5f, 1.0f));
    }

    g.setColour (ink (0.13f));
    g.fillRoundedRectangle (cx - 3.0f, top, 6.0f, faderTravel, 3.0f);

    if (bot - y > 1.0f)
    {
        g.setColour (colour);
        g.fillRoundedRectangle (cx - 3.0f, y, 6.0f, bot - y, 3.0f);
    }

    g.setColour (hue::ink);
    g.fillRect (juce::Rectangle<float> (cx - 7.0f, y - 2.0f, 14.0f, 4.0f));

    text (g, value, { x - 4.0f, faderSvgTop (labelTop) + 120.0f + 6.0f, faderW + 8.0f, 11.0f }, 9.5f,
          textHue (colour), juce::Justification::centred, true);
}

void latch (juce::Graphics& g, juce::Rectangle<float> r, bool on, juce::Colour colour,
            const juce::String& lab, float size)
{
    g.setColour (on ? colour : hue::paper);
    g.fillRect (r);
    g.setColour (on ? colour : ink (0.28f));
    g.drawRect (r, 1.2f);
    text (g, lab, r, size, on ? hue::paper : ink (0.62f));
}

int SelectorLayout::indexAt (juce::Point<float> p) const
{
    for (size_t i = 0; i < seg.size(); ++i)
        if (seg[i].contains (p))
            return (int) i;
    return -1;
}

SelectorLayout SelectorLayout::translated (float dx, float dy) const
{
    SelectorLayout l = *this;
    l.outer.translate (dx, dy);
    for (auto& s : l.seg)
        s.translate (dx, dy);
    return l;
}

SelectorLayout selectorLayout (const juce::StringArray& options, float size, bool vertical, float x, float y)
{
    SelectorLayout l;
    l.size = size;
    l.vertical = vertical;

    const int n = options.size();
    float widest = 0.0f;
    std::vector<float> tw;
    for (auto& o : options)
    {
        tw.push_back (textWidth (o, size, false, 0.02f));
        widest = juce::jmax (widest, tw.back());
    }

    float cursor = 1.0f;
    for (int i = 0; i < n; ++i)
    {
        const float border = i < n - 1 ? 1.0f : 0.0f;
        if (vertical)
        {
            l.seg.push_back ({ x + 1.0f, y + cursor, widest + 16.0f, 20.0f + border });
            cursor += 20.0f + border;
        }
        else
        {
            l.seg.push_back ({ x + cursor, y + 1.0f, tw[(size_t) i] + 8.0f + border, 20.0f });
            cursor += tw[(size_t) i] + 8.0f + border;
        }
    }

    l.outer = vertical ? juce::Rectangle<float> (x, y, widest + 18.0f, cursor + 1.0f)
                       : juce::Rectangle<float> (x, y, cursor + 1.0f, 22.0f);
    return l;
}

void selector (juce::Graphics& g, const SelectorLayout& l, const juce::StringArray& options, int selected,
               juce::Colour colour)
{
    const int n = (int) l.seg.size();
    for (int i = 0; i < n; ++i)
    {
        const auto s = l.seg[(size_t) i];
        const bool active = i == selected;
        const bool hasBorder = i < n - 1;

        if (active)
        {
            g.setColour (colour);
            g.fillRect (s);
        }
        if (hasBorder)
        {
            g.setColour (ink (0.18f));
            g.fillRect (l.vertical ? juce::Rectangle<float> (s.getX(), s.getBottom() - 1.0f, s.getWidth(), 1.0f)
                                   : juce::Rectangle<float> (s.getRight() - 1.0f, s.getY(), 1.0f, s.getHeight()));
        }

        auto t = s;
        if (hasBorder)
            t = l.vertical ? t.withTrimmedBottom (1.0f) : t.withTrimmedRight (1.0f);
        text (g, options[i], t, l.size, active ? hue::paper : ink (0.62f));
    }

    g.setColour (ink (0.28f));
    g.drawRect (l.outer, 1.0f);
}

void dropdown (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& value, juce::Colour colour,
               bool open)
{
    g.setColour (open ? colour : ink (0.28f));
    g.drawRect (r, 1.0f);

    text (g, value, r.withTrimmedLeft (9.0f).withTrimmedRight (20.0f), 9.5f, colour,
          juce::Justification::centredLeft, true);

    lamp (g, { r.getRight() - 16.0f, r.getY() + 7.5f, 6.0f, 6.0f }, open, colour);
}

void envPlot (juce::Graphics& g, juce::Rectangle<float> r, float a, float d, float s, float rel,
              juce::Colour colour)
{
    const float w = r.getWidth(), h = r.getHeight();
    const float ta = std::sqrt (a) * 40.0f + 4.0f, td = std::sqrt (d) * 40.0f + 4.0f, th = 40.0f,
                tr = std::sqrt (rel) * 50.0f + 4.0f;
    const float k = (w - 20.0f) / (ta + td + th + tr);
    const float y0 = h - 8.0f, y1 = 8.0f, ys = y1 + (y0 - y1) * (1.0f - s);

    g.setColour (hue::paper);
    g.fillRect (r);
    g.setColour (colour.withAlpha (0.6f));
    g.drawRect (r, 1.0f);

    g.setColour (ink (0.13f));
    g.fillRect (juce::Rectangle<float> (r.getX() + 10.0f, r.getY() + y0 - 0.5f, w - 20.0f, 1.0f));

    float x = 10.0f;
    juce::Path p;
    p.startNewSubPath (r.getX() + x, r.getY() + y0);
    x += ta * k;  p.lineTo (r.getX() + x, r.getY() + y1);
    x += td * k;  p.lineTo (r.getX() + x, r.getY() + ys);
    x += th * k;  p.lineTo (r.getX() + x, r.getY() + ys);
    x += tr * k;  p.lineTo (r.getX() + x, r.getY() + y0);
    g.setColour (colour);
    g.strokePath (p, juce::PathStrokeType (1.6f));

    const char* kind = (s < 0.15f && rel < 0.4f) ? "TRANSIENT" : (a > 0.35f && s > 0.5f ? "PAD" : "BLOOM");
    text (g, kind, { r.getRight() - 6.0f - 80.0f, r.getY() + 4.0f, 80.0f, 10.0f }, 8.0f, colour,
          juce::Justification::centredRight);
}

void meter (juce::Graphics& g, juce::Rectangle<float> r, float value, juce::Colour colour)
{
    const float v = juce::jlimit (0.0f, 1.0f, value);

    g.setColour (hue::paper);
    g.fillRect (r);
    g.setColour (ink (0.30f));
    g.drawRect (r, 1.0f);

    if (v > 0.02f)
    {
        g.setColour (colour.withAlpha (0.75f));
        g.fillRect (juce::Rectangle<float> (r.getX() + 1.0f, r.getY() + 1.0f,
                                            juce::jmax (0.0f, v * r.getWidth() - 2.0f), r.getHeight() - 2.0f));
    }
}

void segMeter (juce::Graphics& g, float x, float top, float level, juce::Colour colour, int segments)
{
    constexpr float w = 16.0f, h = 6.0f, gap = 3.0f;
    const int lit = juce::roundToInt (juce::jlimit (0.0f, 1.0f, level) * (float) segments);

    for (int i = 0; i < segments; ++i)
    {
        const juce::Rectangle<float> seg (x, top + (float) i * (h + gap), w, h);
        const bool on = segments - i <= lit;

        if (on)
        {
            g.setColour (i == 0 ? hue::amber : colour);
            g.fillRect (seg);
        }
        else
        {
            g.setColour (hue::paper);
            g.fillRect (seg);
            g.setColour (ink (0.22f));
            g.drawRect (seg, 1.0f);
        }
    }
}

/* ── The dropdown menu ───────────────────────────────────────────────────── */

MenuLookAndFeel::MenuLookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, hue::paper);
    setColour (juce::PopupMenu::textColourId, hue::ink);
}

juce::Font MenuLookAndFeel::getPopupMenuFont() { return mono (9.0f * scale); }

void MenuLookAndFeel::drawPopupMenuBackgroundWithOptions (juce::Graphics& g, int w, int h,
                                                          const juce::PopupMenu::Options&)
{
    g.fillAll (hue::paper);
    g.setColour (accent);
    g.drawRect (0, 0, w, h, 1);
}

void MenuLookAndFeel::drawPopupMenuItemWithOptions (juce::Graphics& g, const juce::Rectangle<int>& area,
                                                    bool isHighlighted, const juce::PopupMenu::Item& item,
                                                    const juce::PopupMenu::Options&)
{
    const bool active = item.isTicked;

    if (active)
    {
        g.setColour (accent);
        g.fillRect (area);
    }
    else if (isHighlighted)
    {
        g.setColour (ink (0.08f));
        g.fillRect (area);
    }

    g.setColour (active ? hue::paper : (isHighlighted ? hue::ink : ink (0.62f)));
    g.setFont (getPopupMenuFont());
    g.drawText (item.text, area.reduced (juce::roundToInt (8.0f * scale), 0), juce::Justification::centredLeft, false);
}

void MenuLookAndFeel::getIdealPopupMenuItemSizeWithOptions (const juce::String& s, bool, int, int& idealWidth,
                                                            int& idealHeight, const juce::PopupMenu::Options&)
{
    idealHeight = juce::roundToInt (21.0f * scale);
    idealWidth = juce::roundToInt ((textWidth (s, 9.0f, false, 0.02f) + 16.0f) * scale);
}

} // namespace spn::panel
