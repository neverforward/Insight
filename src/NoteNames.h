#pragma once

#include <array>

namespace insight {

// A note block stores one byte, 0..24, and plays a semitone higher for each step: 0 is F#3 and 24 is
// F#5, two octaves up. The names below are what the player reads on a piano keyboard, so the panel can
// say "F#4" instead of a number nobody can interpret.
//
// Kept pure (no engine headers) so it can be unit tested without a Minecraft process - see tests/.

inline constexpr int kNoteBlockMin = 0;
inline constexpr int kNoteBlockMax = 24;

inline constexpr std::array<char const*, 25> kNoteBlockNames = {
    "F#3", "G3",  "G#3", "A3",  "A#3", "B3",  "C4",  "C#4", "D4",  "D#4", "E4",  "F4",  "F#4",
    "G4",  "G#4", "A4",  "A#4", "B4",  "C5",  "C#5", "D5",  "D#5", "E5",  "F5",  "F#5",
};

// The note's name, or nullptr when the value is outside 0..24 (a corrupted or future block).
[[nodiscard]] inline constexpr char const* noteBlockName(int note) {
    if (note < kNoteBlockMin || note > kNoteBlockMax) {
        return nullptr;
    }
    return kNoteBlockNames[static_cast<size_t>(note)];
}

} // namespace insight
