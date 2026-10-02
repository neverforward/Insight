#include "Colors.h"

#include <algorithm>
#include <cctype>
#include <string_view>

namespace insight {

namespace {

// Every code Bedrock resolves to a colour. The Java-only formatting codes
// (k, l, o, r) are not here on purpose - see the note in Colors.h.
constexpr std::string_view kColorLetters = "0123456789abcdefghijmnpqstuvw";

char lower(char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }

std::string lowered(std::string const& value) {
    std::string out = value;
    std::transform(out.begin(), out.end(), out.begin(), [](char c) { return lower(c); });
    return out;
}

bool isColorLetter(char c) { return kColorLetters.find(lower(c)) != std::string_view::npos; }

} // namespace

bool isColorCode(std::string const& code) { return code.empty() || (code.size() == 1 && isColorLetter(code[0])); }

std::string const& colorCodeChoices() {
    static std::string const choices{kColorLetters};
    return choices;
}

std::optional<std::string> normalizeColorCode(std::string const& value) {
    auto const text = lowered(value);
    if (text.empty() || text == "none" || text == "off" || text == "default") {
        return std::string{}; // no code: the piece keeps the client's plain colour
    }
    if (text.size() == 1 && isColorLetter(text[0])) {
        return std::string(1, text[0]);
    }
    // "&c" / "#c", and the section sign typed as a single Latin-1 byte
    if (text.size() == 2 && (text[0] == '&' || text[0] == '#' || static_cast<unsigned char>(text[0]) == 0xA7)
        && isColorLetter(text[1])) {
        return std::string(1, text[1]);
    }
    // the section sign is two bytes in UTF-8 ("\xC2\xA7c")
    if (text.size() == 3 && static_cast<unsigned char>(text[0]) == 0xC2
        && static_cast<unsigned char>(text[1]) == 0xA7 && isColorLetter(text[2])) {
        return std::string(1, text[2]);
    }
    return std::nullopt;
}

std::string colorPrefix(std::string const& code) {
    if (code.empty() || !isColorCode(code)) {
        return {};
    }
    return std::string(kSectionSign) + code;
}

std::string colored(std::string const& code, std::string const& text) {
    if (text.empty()) {
        return {};
    }
    return colorPrefix(code) + text;
}

} // namespace insight
