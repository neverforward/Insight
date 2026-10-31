#include "gtest/gtest.h"

#include <set>
#include <string>

#include "NoteNames.h"

namespace insight::test {

// A note block stores one byte, 0..24, and plays a semitone higher for each step: 0 is F#3 and 24 is
// F#5, two octaves up.

TEST(NoteBlockNames, CountsSemitonesFromFSharp3) {
    EXPECT_STREQ(noteBlockName(0), "F#3");
    EXPECT_STREQ(noteBlockName(1), "G3");
    EXPECT_STREQ(noteBlockName(6), "C4");  // six semitones above F#3 is the C of the next octave
    EXPECT_STREQ(noteBlockName(12), "F#4");
    EXPECT_STREQ(noteBlockName(18), "C5");
    EXPECT_STREQ(noteBlockName(24), "F#5");
}

TEST(NoteBlockNames, RejectsValuesOutsideTheRange) {
    EXPECT_EQ(noteBlockName(kNoteBlockMin - 1), nullptr);
    EXPECT_EQ(noteBlockName(kNoteBlockMax + 1), nullptr);
    EXPECT_EQ(noteBlockName(-1000), nullptr);
}

TEST(NoteBlockNames, EveryNoteInRangeHasItsOwnName) {
    std::set<std::string> names;
    for (int note = kNoteBlockMin; note <= kNoteBlockMax; ++note) {
        char const* name = noteBlockName(note);
        ASSERT_NE(name, nullptr) << "note " << note << " has no name";
        names.emplace(name);
    }
    // 25 notes, 25 names: a duplicated name would be a transposition nobody notices by ear
    EXPECT_EQ(names.size(), static_cast<size_t>(kNoteBlockMax - kNoteBlockMin + 1));
}

} // namespace insight::test
