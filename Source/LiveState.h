#pragma once

#include <atomic>
#include <cstdint>

/** Everything the panel animates, published from the audio thread and read by
 *  the editor's timer. Plain atomics: the editor tolerates a torn read between
 *  fields, it is only a display. */
struct LiveState
{
    // Detection
    std::atomic<int>   detectedMidi  { -1 };     // -1 when no note is held
    std::atomic<float> detectedCents { 0.0f };

    // Harmony: bumped each time the chord changes, so the editor can keep a history
    std::atomic<uint32_t> chordSerial { 0 };
    std::atomic<int>   effectiveRootPc { 9 };    // the root in use (hand-set or tracked)
    std::atomic<int>   chordRootPc   { 0 };
    std::atomic<int>   chordStyle    { 0 };      // spn::ChordStyle as int
    std::atomic<int>   chordCount    { 0 };
    std::atomic<int>   chordMidi[6]  {};         // lowest first

    // Levels
    std::atomic<float> inLevel   { 0.0f };       // 0..1 peak of the input
    std::atomic<float> outLevel  { 0.0f };       // 0..1 peak of the output
    std::atomic<float> limGrDb   { 0.0f };       // limiter gain reduction, 0 when idle
    std::atomic<float> envLevel  { 0.0f };       // resonator ADSR level, 0..1
    std::atomic<bool>  gateOpen  { false };      // input above GATE
};
