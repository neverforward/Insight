#include "Colors.h"

#include <algorithm>
#include <cctype>
#include <string_view>
#include <utility>

#include "I18n.h"

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

bool colorRgb(std::string const& code, float& r, float& g, float& b) {
    // The palette of the codes above, in the same order.
    static constexpr std::pair<char, unsigned> kPalette[] = {
        {'0', 0x000000u}, {'1', 0x0000AAu}, {'2', 0x00AA00u}, {'3', 0x00AAAAu}, {'4', 0xAA0000u},
        {'5', 0xAA00AAu}, {'6', 0xFFAA00u}, {'7', 0xAAAAAAu}, {'8', 0x555555u}, {'9', 0x5555FFu},
        {'a', 0x55FF55u}, {'b', 0x55FFFFu}, {'c', 0xFF5555u}, {'d', 0xFF55FFu}, {'e', 0xFFFF55u},
        {'f', 0xFFFFFFu}, {'g', 0xDDD605u}, {'h', 0xD9CCB8u}, {'i', 0xA9B4B7u}, {'j', 0x8F727Du},
        {'m', 0xEE222Cu}, {'n', 0xC87363u}, {'p', 0xFFBF1Eu}, {'q', 0x13A045u}, {'s', 0x5FECFFu},
        {'t', 0x577BFFu}, {'u', 0xB66CDDu}, {'v', 0xFF6A00u}, {'w', 0x8CB3FFu},
    };
    if (code.size() != 1) {
        return false;
    }
    char const wanted = lower(code[0]);
    for (auto const& [letter, rgb] : kPalette) {
        if (letter == wanted) {
            r = static_cast<float>((rgb >> 16) & 0xFFu) / 255.0f;
            g = static_cast<float>((rgb >> 8) & 0xFFu) / 255.0f;
            b = static_cast<float>(rgb & 0xFFu) / 255.0f;
            return true;
        }
    }
    return false;
}

std::string colored(std::string const& code, std::string const& text) {
    if (text.empty()) {
        return {};
    }
    return colorPrefix(code) + text;
}

std::string colorDisplayName(std::string const& code, std::string const& locale) {
    // The name of every code this mod accepts, in the order of the palette above.
    static constexpr std::pair<char, char const*> kNames[] = {
        {'\0', "None"         },
        {'0',  "Black"        },
        {'1',  "Dark blue"    },
        {'2',  "Dark green"   },
        {'3',  "Dark aqua"    },
        {'4',  "Dark red"     },
        {'5',  "Dark purple"  },
        {'6',  "Gold"         },
        {'7',  "Gray"         },
        {'8',  "Dark gray"    },
        {'9',  "Blue"         },
        {'a',  "Green"        },
        {'b',  "Aqua"         },
        {'c',  "Red"          },
        {'d',  "Light purple" },
        {'e',  "Yellow"       },
        {'f',  "White"        },
        {'g',  "Minecoin gold"},
        {'h',  "Quartz"       },
        {'i',  "Iron"         },
        {'j',  "Netherite"    },
        {'m',  "Redstone"     },
        {'n',  "Copper"       },
        {'p',  "Gold ingot"   },
        {'q',  "Emerald"      },
        {'s',  "Diamond"      },
        {'t',  "Lapis"        },
        {'u',  "Amethyst"     },
        {'v',  "Resin"        },
        {'w',  "Party blue"   },
    };
    char const wanted = code.empty() ? '\0' : lower(code[0]);
    for (auto const& [letter, key] : kNames) {
        if (letter == wanted) {
            return tr(locale, key);
        }
    }
    return code;
}

} // namespace insight
