#pragma once

/* Parameter IDs. The panel and the processor both read these; never write the
 * literal strings anywhere else. */
namespace spn::pid
{
// Input
inline constexpr const char* in      = "in";        // float  -12..12 dB, 0
inline constexpr const char* gate    = "gate";      // float  -72..-24 dB, -58
inline constexpr const char* range   = "range";     // choice SMALL MEDIUM LARGE, 1
// Harmony
inline constexpr const char* root    = "root";      // choice C..B, 9 (A)
inline constexpr const char* track   = "track";     // bool, off
inline constexpr const char* follows = "follows";   // choice NOTE KEY, 1
inline constexpr const char* scale   = "scale";     // choice, 11 scales, 1 (MINOR)
inline constexpr const char* chord   = "chord";     // choice, 10 styles, 1 (7TH)
inline constexpr const char* random  = "random";    // bool, on
inline constexpr const char* voices  = "voices";    // choice 2..6, index 3 (= 5 voices)
// Progression
inline constexpr const char* mode    = "mode";      // choice FOLLOW CADENCE WANDER, 1
inline constexpr const char* tension = "tension";   // float 0..1, 0.35
inline constexpr const char* hold    = "hold";      // float 50..1000 ms, 300
// Resonator
inline constexpr const char* ring    = "ring";      // float 0.2..8 s, 4.5
inline constexpr const char* damp    = "damp";      // float 1000..12000 Hz, 6500
inline constexpr const char* spread  = "spread";    // float 0..3 oct, 1.2
inline constexpr const char* glide   = "glide";     // float 0..200 ms, 40
inline constexpr const char* atk     = "atk";       // float 1..2000 ms, 30
inline constexpr const char* dec     = "dec";       // float 10..2000 ms, 413
inline constexpr const char* sus     = "sus";       // float 0..1, 0.6
inline constexpr const char* rel     = "rel";       // float 20..8000 ms, 2000
// Output
inline constexpr const char* mix     = "mix";       // float 0..1, 0.6
inline constexpr const char* lim     = "lim";       // bool, on
}
