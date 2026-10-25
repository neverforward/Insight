#include "ConfigUi.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <Windows.h> // virtual-key codes

#include <imgui.h>

#include "Colors.h"
#include "Config.h"
#include "I18n.h"
#include "ImGuiOverlay.h"
#include "Insight.h"
#include "Util.h"

namespace insight {

namespace {

// ---------------------------------------------------------------------------
// input translation: Windows virtual keys -> ImGui keys (no ImGui Win32
// backend is installed, so the mapping lives here)
// ---------------------------------------------------------------------------
[[maybe_unused]] ImGuiKey toImGuiKey(int vk) {
    if (vk >= 'A' && vk <= 'Z') {
        return static_cast<ImGuiKey>(ImGuiKey_A + (vk - 'A'));
    }
    if (vk >= '0' && vk <= '9') {
        return static_cast<ImGuiKey>(ImGuiKey_0 + (vk - '0'));
    }
    if (vk >= VK_F1 && vk <= VK_F12) {
        return static_cast<ImGuiKey>(ImGuiKey_F1 + (vk - VK_F1));
    }
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
        return static_cast<ImGuiKey>(ImGuiKey_Keypad0 + (vk - VK_NUMPAD0));
    }
    switch (vk) {
    case VK_TAB:
        return ImGuiKey_Tab;
    case VK_LEFT:
        return ImGuiKey_LeftArrow;
    case VK_RIGHT:
        return ImGuiKey_RightArrow;
    case VK_UP:
        return ImGuiKey_UpArrow;
    case VK_DOWN:
        return ImGuiKey_DownArrow;
    case VK_PRIOR:
        return ImGuiKey_PageUp;
    case VK_NEXT:
        return ImGuiKey_PageDown;
    case VK_HOME:
        return ImGuiKey_Home;
    case VK_END:
        return ImGuiKey_End;
    case VK_INSERT:
        return ImGuiKey_Insert;
    case VK_DELETE:
        return ImGuiKey_Delete;
    case VK_BACK:
        return ImGuiKey_Backspace;
    case VK_SPACE:
        return ImGuiKey_Space;
    case VK_RETURN:
        return ImGuiKey_Enter;
    case VK_ESCAPE:
        return ImGuiKey_Escape;
    case VK_OEM_7:
        return ImGuiKey_Apostrophe;
    case VK_OEM_COMMA:
        return ImGuiKey_Comma;
    case VK_OEM_MINUS:
        return ImGuiKey_Minus;
    case VK_OEM_PERIOD:
        return ImGuiKey_Period;
    case VK_OEM_2:
        return ImGuiKey_Slash;
    case VK_OEM_1:
        return ImGuiKey_Semicolon;
    case VK_OEM_PLUS:
        return ImGuiKey_Equal;
    case VK_OEM_4:
        return ImGuiKey_LeftBracket;
    case VK_OEM_5:
        return ImGuiKey_Backslash;
    case VK_OEM_6:
        return ImGuiKey_RightBracket;
    case VK_OEM_3:
        return ImGuiKey_GraveAccent;
    case VK_SHIFT:
    case VK_LSHIFT:
        return ImGuiKey_LeftShift;
    case VK_CONTROL:
    case VK_LCONTROL:
        return ImGuiKey_LeftCtrl;
    case VK_MENU:
    case VK_LMENU:
        return ImGuiKey_LeftAlt;
    case VK_RSHIFT:
        return ImGuiKey_RightShift;
    case VK_RCONTROL:
        return ImGuiKey_RightCtrl;
    case VK_RMENU:
        return ImGuiKey_RightAlt;
    default:
        return ImGuiKey_None;
    }
}

// printable character for a virtual key (US layout; enough for the format
// strings edited in the screen)
[[maybe_unused]] char charFor(int vk, bool shift) {
    if (vk >= 'A' && vk <= 'Z') {
        return static_cast<char>(shift ? vk : vk + ('a' - 'A'));
    }
    if (vk >= '0' && vk <= '9') {
        static constexpr char shifted[] = ")!@#$%^&*(";
        return shift ? shifted[vk - '0'] : static_cast<char>(vk);
    }
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
        return static_cast<char>('0' + (vk - VK_NUMPAD0));
    }
    switch (vk) {
    case VK_SPACE:
        return ' ';
    case VK_OEM_MINUS:
        return static_cast<char>(shift ? '_' : '-');
    case VK_OEM_PLUS:
        return static_cast<char>(shift ? '+' : '=');
    case VK_OEM_4:
        return static_cast<char>(shift ? '{' : '[');
    case VK_OEM_6:
        return static_cast<char>(shift ? '}' : ']');
    case VK_OEM_1:
        return static_cast<char>(shift ? ':' : ';');
    case VK_OEM_7:
        return static_cast<char>(shift ? '"' : '\'');
    case VK_OEM_COMMA:
        return static_cast<char>(shift ? '<' : ',');
    case VK_OEM_PERIOD:
        return static_cast<char>(shift ? '>' : '.');
    case VK_OEM_2:
        return static_cast<char>(shift ? '?' : '/');
    case VK_OEM_5:
        return static_cast<char>(shift ? '|' : '\\');
    case VK_OEM_3:
        return static_cast<char>(shift ? '~' : '`');
    default:
        return 0;
    }
}

std::string keyName(int vk) {
    if (vk == 0) {
        return {};
    }
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) {
        return std::string(1, static_cast<char>(vk));
    }
    if (vk >= VK_F1 && vk <= VK_F12) {
        return "F" + std::to_string(vk - VK_F1 + 1);
    }
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
        return "Num" + std::string(1, static_cast<char>('0' + (vk - VK_NUMPAD0)));
    }
    switch (vk) {
    case VK_SPACE:
        return "Space";
    case VK_TAB:
        return "Tab";
    case VK_RETURN:
        return "Enter";
    case VK_ESCAPE:
        return "Esc";
    case VK_BACK:
        return "Backspace";
    case VK_INSERT:
        return "Insert";
    case VK_DELETE:
        return "Delete";
    case VK_HOME:
        return "Home";
    case VK_END:
        return "End";
    case VK_PRIOR:
        return "PageUp";
    case VK_NEXT:
        return "PageDown";
    case VK_LEFT:
        return "Left";
    case VK_RIGHT:
        return "Right";
    case VK_UP:
        return "Up";
    case VK_DOWN:
        return "Down";
    case VK_SHIFT:
    case VK_LSHIFT:
        return "Shift";
    case VK_CONTROL:
    case VK_LCONTROL:
        return "Ctrl";
    case VK_MENU:
    case VK_LMENU:
        return "Alt";
    default:
        char buf[16];
        std::snprintf(buf, sizeof(buf), "0x%02X", vk);
        return buf;
    }
}

// one editable setting
enum class Kind { Bool, Float, Int, Text, Enum, Key };

struct Row {
    std::string              label;  // i18n key
    std::string              option; // name accepted by applyConfigEdit
    Kind                     kind      = Kind::Bool;
    float                    lo        = 0.0f;
    float                    hi        = 1.0f;
    bool                     multiline = false;
    std::vector<std::string> choices; // Enum
};

std::string boolText(std::string const& locale, bool value) { return value ? tr(locale, "On") : tr(locale, "Off"); }

// Every boolean option in one table. A row whose value cannot be read renders as
// an unchecked box that then refuses to turn off (it only ever queues "true"), so
// every switch has to be listed here - the extras block alone has two dozen.
bool const* findBoolOption(std::string const& option) {
    auto const& cfg = Insight::cfg();
    struct Entry {
        std::string_view name;
        bool const*      value;
    };
    static Entry const table[] = {
        {"enabled",                &cfg.enabled                },
        {"passThroughLiquids",     &cfg.passThroughLiquids     },
        {"showEmpty",              &cfg.showEmpty              },
        {"entityEnabled",          &cfg.entityEnabled          },
        {"display.name",           &cfg.display.name           },
        {"display.facing",         &cfg.display.facing         },
        {"display.identifier",     &cfg.display.identifier     },
        {"display.translationKey", &cfg.display.translationKey },
        {"display.position",       &cfg.display.position       },
        {"display.distance",       &cfg.display.distance       },
        {"display.light",          &cfg.display.light          },
        {"display.emission",       &cfg.display.emission       },
        {"display.extras",         &cfg.display.extras         },
        {"entity.name",            &cfg.entity.name            },
        {"entity.facing",          &cfg.entity.facing          },
        {"entity.identifier",      &cfg.entity.identifier      },
        {"entity.translationKey",  &cfg.entity.translationKey  },
        {"entity.position",        &cfg.entity.position        },
        {"entity.distance",        &cfg.entity.distance        },
        {"entity.health",          &cfg.entity.health          },
        {"entity.extras",          &cfg.entity.extras          },
        {"entity.healthDecimals",  &cfg.entity.healthDecimals  },
        {"entity.armor",           &cfg.entity.armor           },
        {"showOverlay",            &cfg.client.showOverlay     },
        {"hideOverlayInGui",       &cfg.client.hideOverlayInGui},
        {"background",             &cfg.client.background      },
        {"shadow",                 &cfg.client.shadow          },
        {"extras.enabled",         &cfg.extras.enabled         },
        {"extras.hardness",        &cfg.extras.hardness        },
        {"extras.blastResistance", &cfg.extras.blastResistance },
        {"extras.chest",           &cfg.extras.chest           },
        {"extras.chestIcons",      &cfg.extras.chestIcons      },
        {"extras.chestIconsMerge", &cfg.extras.chestIconsMerge },
        {"extras.bookshelf",       &cfg.extras.bookshelf       },
        {"extras.shelf",           &cfg.extras.shelf           },
        {"extras.lectern",         &cfg.extras.lectern         },
        {"extras.pot",             &cfg.extras.pot             },
        {"extras.brewing",         &cfg.extras.brewing         },
        {"extras.furnace",         &cfg.extras.furnace         },
        {"extras.jukebox",         &cfg.extras.jukebox         },
        {"extras.sign",            &cfg.extras.sign            },
        {"extras.banner",          &cfg.extras.banner          },
        {"extras.itemFrame",       &cfg.extras.itemFrame       },
        {"extras.flowerPot",       &cfg.extras.flowerPot       },
        {"extras.painting",        &cfg.extras.painting        },
        {"extras.piston",          &cfg.extras.piston          },
        {"extras.redstone",        &cfg.extras.redstone        },
        {"extras.repeater",        &cfg.extras.repeater        },
        {"extras.comparator",      &cfg.extras.comparator      },
        {"extras.dispenser",       &cfg.extras.dispenser       },
        {"extras.candle",          &cfg.extras.candle          },
        {"extras.respawnAnchor",   &cfg.extras.respawnAnchor   },
        {"extras.misc",            &cfg.extras.misc            },
        {"entityExtras.owner",      &cfg.entityExtras.owner     },
        {"entityExtras.effects",    &cfg.entityExtras.effects   },
        {"entityExtras.breeding",   &cfg.entityExtras.breeding  },
        {"entityExtras.conversion", &cfg.entityExtras.conversion},
        {"entityExtras.horse",      &cfg.entityExtras.horse     },
        {"entityExtras.armorStand", &cfg.entityExtras.armorStand},
        {"entityExtras.villager",   &cfg.entityExtras.villager  },
        {"entityExtras.waxed",      &cfg.entityExtras.waxed     },
    };
    for (auto const& entry : table) {
        if (option == entry.name) {
            return entry.value;
        }
    }
    return nullptr;
}

// Every colour option in one table, for the same reason the switches have one: a
// row whose value cannot be read shows a colour that is not the configured one.
std::string const* findColorOption(std::string const& option) {
    auto const& cfg = Insight::cfg();
    struct Entry {
        std::string_view    name;
        std::string const*  value;
    };
    static Entry const table[] = {
        {"colors.name",           &cfg.colors.name          },
        {"colors.facing",         &cfg.colors.facing        },
        {"colors.identifier",     &cfg.colors.identifier    },
        {"colors.translationKey", &cfg.colors.translationKey},
        {"colors.x",              &cfg.colors.x             },
        {"colors.y",              &cfg.colors.y             },
        {"colors.z",              &cfg.colors.z             },
        {"colors.distance",       &cfg.colors.distance      },
        {"colors.label",          &cfg.colors.label         },
        {"colors.value",          &cfg.colors.value         },
        {"colors.health",         &cfg.colors.health        },
    };
    for (auto const& entry : table) {
        if (option == entry.name) {
            return entry.value;
        }
    }
    return nullptr;
}

// The colours a piece of the panel can be given: "no code" first, then every code
// Bedrock resolves to a colour (the 0-9/a-f set plus the material colours only
// Bedrock has).
std::vector<std::string> const& colorChoices() {
    static std::vector<std::string> const choices = [] {
        std::vector<std::string> out;
        out.emplace_back();
        for (char const code : colorCodeChoices()) {
            out.emplace_back(1, code);
        }
        return out;
    }();
    return choices;
}

std::string currentValueText(Row const& row, std::string const& locale) {
    auto const& cfg = Insight::cfg();

    // colours first: they are plain strings and would otherwise fall through to
    // the empty return below, which reads as "no colour" for every row
    if (auto const* value = findColorOption(row.option)) {
        return *value;
    }

    if (row.kind == Kind::Bool) {
        // table first, so a switch that was added later still reports its state
        if (auto const* value = findBoolOption(row.option)) {
            return boolText(locale, *value);
        }
        if (row.option == "enabled") {
            return boolText(locale, cfg.enabled);
        }
        if (row.option == "showOverlay") {
            return boolText(locale, cfg.client.showOverlay);
        }
        if (row.option == "hideOverlayInGui") {
            return boolText(locale, cfg.client.hideOverlayInGui);
        }
        if (row.option == "passThroughLiquids") {
            return boolText(locale, cfg.passThroughLiquids);
        }
        if (row.option == "showEmpty") {
            return boolText(locale, cfg.showEmpty);
        }
        if (row.option == "entityEnabled") {
            return boolText(locale, cfg.entityEnabled);
        }
        if (row.option == "background") {
            return boolText(locale, cfg.client.background);
        }
        if (row.option == "shadow") {
            return boolText(locale, cfg.client.shadow);
        }
        if (row.option == "extras.enabled") {
            return boolText(locale, cfg.extras.enabled);
        }
        if (row.option == "extras.chest") {
            return boolText(locale, cfg.extras.chest);
        }
        if (row.option == "extras.furnace") {
            return boolText(locale, cfg.extras.furnace);
        }
        if (row.option == "extras.brewing") {
            return boolText(locale, cfg.extras.brewing);
        }
        if (row.option == "extras.redstone") {
            return boolText(locale, cfg.extras.redstone);
        }
        if (row.option == "extras.misc") {
            return boolText(locale, cfg.extras.misc);
        }
    }
    if (row.option == "maxDistance") {
        return util::trimNumber(cfg.maxDistance, 1);
    }
    if (row.option == "intervalTicks") {
        return std::to_string(cfg.intervalTicks);
    }
    if (row.option == "entity.healthStyle") {
        return cfg.entity.healthStyle;
    }
    if (row.option == "entity.heartsThreshold") {
        return util::trimNumber(cfg.entity.heartsThreshold, 1);
    }
    if (row.option == "entity.heartsPerRow") {
        return std::to_string(cfg.entity.heartsPerRow);
    }
    if (row.option == "extras.chestIconsPerRow") {
        return std::to_string(cfg.extras.chestIconsPerRow);
    }
    if (row.option == "extras.chestIconLimit") {
        return std::to_string(cfg.extras.chestIconLimit);
    }
    if (row.option == "entityExtras.effectsLimit") {
        return std::to_string(cfg.entityExtras.effectsLimit);
    }
    if (row.option == "entityExtras.petArmor") {
        return cfg.entityExtras.petArmor;
    }
    if (row.option == "offsetX") {
        return util::trimNumber(cfg.client.offsetX, 2);
    }
    if (row.option == "offsetY") {
        return util::trimNumber(cfg.client.offsetY, 2);
    }
    if (row.option == "fontSize") {
        return util::trimNumber(cfg.client.fontSize, 2);
    }
    if (row.option == "maxWidth") {
        return util::trimNumber(cfg.client.maxWidth, 2);
    }
    if (row.option == "transitionTime") {
        return util::trimNumber(cfg.client.transitionTime, 2);
    }
    if (row.option == "backgroundAlpha") {
        return util::trimNumber(cfg.client.backgroundAlpha, 2);
    }
    if (row.option == "language") {
        return cfg.client.language;
    }
    if (row.option == "textColor") {
        return cfg.client.textColor;
    }
    if (row.option == "anchor") {
        return cfg.client.anchor;
    }
    if (row.option == "overlayOnRemote") {
        return cfg.client.overlayOnRemote;
    }
    if (row.option == "emptyText") {
        return cfg.emptyText;
    }
    if (row.option == "keyOpenConfig") {
        return keyName(cfg.client.keyOpenConfig);
    }
    if (row.option == "keyToggleShow") {
        return keyName(cfg.client.keyToggleShow);
    }
    return {};
}

// Fixed choices are stored as tokens ("top_left", "on", "auto") because that is
// what the config file and /insight set accept; the screen shows a localized
// label for them instead of the raw token.
/// The languages this mod actually ships messages for, plus "auto"; the list is
/// read from the message folder once, so adding lang/<code>.json is enough.
std::vector<std::string> const& availableLanguages() {
    static std::vector<std::string> const codes = [] {
        std::vector<std::string> out{"auto"};
        std::error_code          error;
        auto const               dir = Insight::getInstance().getSelf().getLangDir();
        for (auto const& entry : std::filesystem::directory_iterator(dir, error)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                out.push_back(entry.path().stem().string());
            }
        }
        std::sort(out.begin() + 1, out.end());
        return out;
    }();
    return codes;
}

std::string choiceLabel(std::string const& value, std::string const& locale) {
    static constexpr std::pair<char const*, char const*> kChoices[] = {
        {"top_left",      "Top left"     },
        {"top_center",    "Top center"   },
        {"top_right",     "Top right"    },
        {"middle_left",   "Middle left"  },
        {"center",        "Center"       },
        {"middle_right",  "Middle right" },
        {"bottom_left",   "Bottom left"  },
        {"bottom_center", "Bottom center"},
        {"bottom_right",  "Bottom right" },
        {"on",            "On"           },
        {"off",           "Off"          },
        {"auto",          "Auto"         },
        {"bar",           "Bar"          },
        {"bar+number",    "Bar + number" },
        {"hearts",        "Hearts"       },
        {"hearts+number", "Hearts + number"},
        {"number",        "Number"       },
        {"all",           "All"          },
        {"damaged",       "Damaged"      },
    };
    // shipped message files are named after their locale: show them natively
    static constexpr std::pair<char const*, char const*> kNativeNames[] = {
        {"en",    "English"                 },
        {"zh_cn", "\u7b80\u4f53\u4e2d\u6587"},
    };
    for (auto const& [code, native] : kNativeNames) {
        if (value == code) {
            return native;
        }
    }
    for (auto const& [token, key] : kChoices) {
        if (value == token) {
            return tr(locale, key);
        }
    }
    return value;
}

// The colour rows store a single formatting code, so their value ("c") is shown as the
// colour's name (colorDisplayName, shared with the server's menu). This deliberately lives
// apart from choiceLabel(): that one is used for every row's value column, and mapping bare
// "1"/"c" there renamed ordinary numbers - a font size of 1 came out as "Dark blue".

// Is this the option name of a colour row? Only those resolve a value to a colour name.
bool isColorRow(std::string const& option) { return option.rfind("colors.", 0) == 0; }

// How one row's value is written out: colour names for the colour rows, the fixed
// token names for the other choices, the value itself otherwise.
std::string valueLabel(Row const& row, std::string const& value, std::string const& locale) {
    return isColorRow(row.option) ? colorDisplayName(value, locale) : choiceLabel(value, locale);
}

bool rowIsOn(Row const& row, std::string const& locale) { return currentValueText(row, locale) == tr(locale, "On"); }

// A switch: a rounded track with a knob that slides to the right when it is on. ImGui has no switch
// of its own, and a checkbox reads as "tick this box" rather than "this is on" - on a settings page
// the second reading is the one that matters. Small on purpose (a line of text high), and flat: the
// hover is spent on the cursor changing, not on the colours.
constexpr ImU32 kSwitchOnTrack  = IM_COL32(0xe5, 0xe5, 0xe5, 255);
constexpr ImU32 kSwitchOnKnob   = IM_COL32(0x17, 0x17, 0x17, 255);
constexpr ImU32 kSwitchOffTrack = IM_COL32(0x28, 0x28, 0x28, 255);
constexpr ImU32 kSwitchOffKnob  = IM_COL32(0xfa, 0xfa, 0xfa, 255);

constexpr float kSwitchWidth  = 2.0f; // multiplier of the height
constexpr float kSwitchHeight = 1.2f; // multiplier of the text line height

bool toggleSwitch(char const* id, bool* value) {
    float const height = ImGui::GetTextLineHeight() * kSwitchHeight;
    float const width  = height * kSwitchWidth;
    float const radius = height * 0.5f;

    ImVec2 const origin  = ImGui::GetCursorScreenPos();
    bool const   pressed = ImGui::InvisibleButton(id, ImVec2(width, height));
    if (pressed) {
        *value = !*value;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }

    ImDrawList* draw  = ImGui::GetWindowDrawList();
    ImU32 const track = *value ? kSwitchOnTrack : kSwitchOffTrack;
    ImU32 const knob  = *value ? kSwitchOnKnob : kSwitchOffKnob;
    float const knobX = *value ? origin.x + width - radius : origin.x + radius;

    draw->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + height), track, radius);
    draw->AddCircleFilled(ImVec2(knobX, origin.y + radius), radius - 1.5f, knob);
    return pressed;
}


// Theme of the configuration screen, after shadcn/ui's zinc palette: one neutral ramp, 1px borders,
// 10px radii, no gradients and no second accent colour - the accent is near-white on near-black, the
// way a shadcn primary button is.
void applyStyle() {
    ImGuiStyle& s = ImGui::GetStyle();
    ImGui::StyleColorsDark(&s);

    s.WindowRounding          = 10.0f;
    s.ChildRounding           = 10.0f;
    s.FrameRounding           = 8.0f;
    s.PopupRounding           = 10.0f;
    s.ScrollbarRounding       = 8.0f;
    s.GrabRounding            = 8.0f;
    s.TabRounding             = 8.0f;
    s.WindowBorderSize        = 1.0f;
    s.ChildBorderSize         = 1.0f;
    s.FrameBorderSize         = 1.0f;
    s.PopupBorderSize         = 1.0f;
    s.WindowPadding           = ImVec2(16.0f, 14.0f);
    s.FramePadding            = ImVec2(10.0f, 6.0f);
    s.ItemSpacing             = ImVec2(10.0f, 8.0f);
    s.ItemInnerSpacing        = ImVec2(8.0f, 6.0f);
    s.IndentSpacing           = 16.0f;
    s.ScrollbarSize           = 10.0f;
    s.GrabMinSize             = 12.0f;
    s.WindowTitleAlign        = ImVec2(0.0f, 0.5f);
    s.SeparatorTextBorderSize = 1.0f;
    s.SeparatorTextPadding    = ImVec2(0.0f, 10.0f);

    auto rgb = [](int r, int g, int b, float a = 1.0f) {
        return ImVec4(
            static_cast<float>(r) / 255.0f,
            static_cast<float>(g) / 255.0f,
            static_cast<float>(b) / 255.0f,
            a
        );
    };
    // zinc-950 / zinc-900 / zinc-800 / zinc-700 / zinc-400 / zinc-100 / zinc-50
    ImVec4 const bg      = rgb(0x09, 0x09, 0x0b, 0.98f);
    ImVec4 const card    = rgb(0x09, 0x09, 0x0b);
    ImVec4 const popup   = rgb(0x11, 0x11, 0x13, 0.98f);
    ImVec4 const muted   = rgb(0x18, 0x18, 0x1b);
    ImVec4 const border  = rgb(0x27, 0x27, 0x2a);
    ImVec4 const hover   = rgb(0x27, 0x27, 0x2a);
    ImVec4 const active  = rgb(0x3f, 0x3f, 0x46);
    ImVec4 const dim     = rgb(0xa1, 0xa1, 0xaa);
    ImVec4 const strong  = rgb(0xfa, 0xfa, 0xfa);
    ImVec4 const accent  = rgb(0xd4, 0xd4, 0xd8);

    ImVec4* col                        = s.Colors;
    col[ImGuiCol_Text]                 = strong;
    col[ImGuiCol_TextDisabled]         = dim;
    col[ImGuiCol_WindowBg]             = bg;
    col[ImGuiCol_ChildBg]              = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    col[ImGuiCol_PopupBg]              = popup;
    col[ImGuiCol_Border]               = border;
    col[ImGuiCol_BorderShadow]         = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    col[ImGuiCol_FrameBg]              = muted;
    col[ImGuiCol_FrameBgHovered]       = hover;
    col[ImGuiCol_FrameBgActive]        = active;
    col[ImGuiCol_TitleBg]              = card;
    col[ImGuiCol_TitleBgActive]        = card;
    col[ImGuiCol_TitleBgCollapsed]     = card;
    col[ImGuiCol_MenuBarBg]            = card;
    col[ImGuiCol_ScrollbarBg]          = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    col[ImGuiCol_ScrollbarGrab]        = border;
    col[ImGuiCol_ScrollbarGrabHovered] = active;
    col[ImGuiCol_ScrollbarGrabActive]  = accent;
    col[ImGuiCol_CheckMark]            = strong;
    col[ImGuiCol_SliderGrab]           = strong;
    col[ImGuiCol_SliderGrabActive]     = accent;
    col[ImGuiCol_Button]               = muted;
    col[ImGuiCol_ButtonHovered]        = hover;
    col[ImGuiCol_ButtonActive]         = active;
    col[ImGuiCol_Header]               = muted;
    col[ImGuiCol_HeaderHovered]        = hover;
    col[ImGuiCol_HeaderActive]         = active;
    col[ImGuiCol_Separator]            = border;
    col[ImGuiCol_SeparatorHovered]     = active;
    col[ImGuiCol_SeparatorActive]      = accent;
    col[ImGuiCol_Tab]                  = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    col[ImGuiCol_TabHovered]           = muted;
    col[ImGuiCol_TabSelected]          = muted;
    col[ImGuiCol_TabSelectedOverline]  = accent;
    col[ImGuiCol_TabDimmed]            = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    col[ImGuiCol_TabDimmedSelected]    = muted;
    col[ImGuiCol_TableHeaderBg]        = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    col[ImGuiCol_TableBorderStrong]    = border;
    col[ImGuiCol_TableBorderLight]     = border;
    col[ImGuiCol_TableRowBg]           = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    col[ImGuiCol_TableRowBgAlt]        = ImVec4(1.0f, 1.0f, 1.0f, 0.015f);
    col[ImGuiCol_TextSelectedBg]       = rgb(0x3f, 0x3f, 0x46, 0.7f);
    col[ImGuiCol_TextLink]             = accent;
    col[ImGuiCol_NavHighlight]         = accent;
    col[ImGuiCol_NavCursor]            = accent;
}

} // namespace

ConfigUi& ConfigUi::instance() {
    static ConfigUi ui;
    return ui;
}

void ConfigUi::setVisible(bool visible) {
    mVisible.store(visible, std::memory_order_release);
    ImGuiOverlay::setInputCaptured(visible);
}

void ConfigUi::toggle() { setVisible(!visible()); }

void ConfigUi::setLocale(std::string locale) {
    std::lock_guard lock(mMutex);
    if (mLocale == locale) {
        return;
    }
    mLocale = std::move(locale);
}


void ConfigUi::setStatus(bool ok, std::string message) {
    std::lock_guard lock(mMutex);
    mStatusOk      = ok;
    mStatusMessage = std::move(message);
}

void ConfigUi::queueEdit(std::string option, std::string value) {
    std::lock_guard lock(mMutex);
    // a queue, not a single slot: two edits between two game-thread ticks must
    // both reach the config instead of the later one overwriting the earlier
    mPending.push_back(Edit{std::move(option), std::move(value)});
}

bool ConfigUi::takeEdit(std::string& option, std::string& value) {
    std::lock_guard lock(mMutex);
    if (mPending.empty()) {
        return false;
    }
    option = std::move(mPending.front().option);
    value  = std::move(mPending.front().value);
    mPending.erase(mPending.begin());
    return true;
}

void ConfigUi::onKey(int vkCode, bool down) {
    std::lock_guard lock(mMutex);
    mEvents.push_back(Event{down ? EventKind::KeyDown : EventKind::KeyUp, vkCode});
}

void ConfigUi::draw() {
    if (!visible()) {
        return;
    }
    // Publish whether the keyboard belongs to the screen (see wantsKeyboard): the text
    // field, if any, has the caret. Set here rather than on the paths out of draw(), so
    // that every early return still leaves the game thread with the truth.
    mWantTextInput.store(ImGui::GetIO().WantTextInput, std::memory_order_relaxed);
    static bool styleApplied = false;
    if (!styleApplied) {
        applyStyle();
        styleApplied = true;
    }

    // --- state handed over by the game thread ----------------------------
    std::vector<Event> events;
    std::string        locale;
    std::string        status;
    bool               statusOk = true;
    {
        std::lock_guard lock(mMutex);
        events.swap(mEvents);
        locale   = mLocale;
        status   = mStatusMessage;
        statusOk = mStatusOk;
    }

    // Input comes from ImGui's Win32 backend (installed by the overlay); only
    // the "press a key to rebind" capture still reads the queued game events.
    for (auto const& e : events) {
        if (e.kind != EventKind::KeyDown) {
            continue;
        }
        if (mCapturingKey >= 0) {
            if (e.code != VK_ESCAPE) {
                queueEdit(mCapturingKey == 0 ? "keyOpenConfig" : "keyToggleShow", std::to_string(e.code));
            }
            mCapturingKey = -1;
        } else if (e.code == VK_ESCAPE) {
            setVisible(false);
        }
    }
    if (mCapturingKey < 0 && ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        setVisible(false); // the backend delivers Esc even if the engine does not
        return;
    }

    // Smaller than the panel it configures, and placed only once: from then on the size the
    // player drags is kept, instead of being reset to a fraction of the screen every frame.
    ImVec2 const display = ImGui::GetIO().DisplaySize;
    ImVec2 const size(std::min(display.x * 0.62f, 1100.0f), std::min(display.y * 0.62f, 780.0f));
    ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, display.y * 0.5f), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    ImGui::Begin(
        tr(locale, "Insight configuration").c_str(),
        nullptr,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
    );

    // the languages this mod ships messages for, plus "auto" (read once)
    std::vector<std::string> const languageChoices = availableLanguages();

    ImGuiStyle const& style   = ImGui::GetStyle();
    float const       footerH = ImGui::GetFrameHeight() + style.ItemSpacing.y * 2.0f;

    // The widget of one setting, drawn inline in the row's value column. Edits are only queued (the
    // game thread owns the config file).
    auto editor = [&](Row const& row, int id) {
        ImGui::PushID(id);
        ImGui::SetNextItemWidth(-1.0f);
        switch (row.kind) {
        case Kind::Bool: {
            bool value = rowIsOn(row, locale);
            // The switch sits against the right edge of the row: that is where the eye looks for a
            // state, and it lines the switches of a group up in one column instead of one per label.
            float const width = ImGui::GetTextLineHeight() * kSwitchHeight * kSwitchWidth;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - width);
            if (toggleSwitch("##value", &value)) {
                queueEdit(row.option, value ? "true" : "false");
            }
            break;
        }
        case Kind::Float:
        case Kind::Int: {
            // The row keeps its own value while it is dragged - ImGui adds the delta to the value it
            // is handed, so re-reading the config every frame would undo the drag - and the edit is
            // queued once, when the drag ends.
            bool const isInt = row.kind == Kind::Int;
            float&     value = mValues[row.option];
            if (mActiveSlider != row.option) {
                value = static_cast<float>(std::atof(currentValueText(row, locale).c_str()));
            }
            if (isInt) {
                int shown = static_cast<int>(std::lround(value));
                if (ImGui::SliderInt("##value", &shown, static_cast<int>(row.lo), static_cast<int>(row.hi))) {
                    value = static_cast<float>(shown);
                }
            } else {
                ImGui::SliderFloat("##value", &value, row.lo, row.hi, "%.2f");
            }
            if (ImGui::IsItemActivated()) {
                mActiveSlider = row.option;
            }
            if (ImGui::IsItemDeactivated() && mActiveSlider == row.option) {
                mActiveSlider.clear();
            }
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                queueEdit(
                    row.option,
                    isInt ? std::to_string(static_cast<int>(std::lround(value))) : util::trimNumber(value, 2)
                );
            }
            break;
        }
        case Kind::Enum: {
            std::string const current = currentValueText(row, locale);
            if (ImGui::BeginCombo("##value", valueLabel(row, current, locale).c_str())) {
                for (auto const& choice : row.choices) {
                    if (ImGui::Selectable(valueLabel(row, choice, locale).c_str(), choice == current)) {
                        queueEdit(row.option, choice); // the config keeps the token
                    }
                }
                ImGui::EndCombo();
            }
            break;
        }
        case Kind::Key: {
            bool const capturing = mCapturingKey == (row.option == "keyOpenConfig" ? 0 : 1);
            if (ImGui::Button(capturing ? tr(locale, "Press a key...").c_str() : tr(locale, "Change key").c_str())) {
                mCapturingKey = row.option == "keyOpenConfig" ? 0 : 1;
            }
            ImGui::SameLine();
            if (ImGui::Button(tr(locale, "Clear").c_str())) {
                queueEdit(row.option, "0");
            }
            break;
        }
        case Kind::Text: {
            // one buffer per row, refilled from the config whenever the row is not the one being
            // typed into: this is what makes the field usable at all, and having one per row is what
            // lets several text rows be visible at once
            auto& buffer = mText[row.option];
            if (mActiveText != row.option) {
                std::snprintf(buffer.data(), buffer.size(), "%s", currentValueText(row, locale).c_str());
            }
            if (row.multiline) {
                ImGui::InputTextMultiline("##value", buffer.data(), buffer.size(), ImVec2(-1.0f, 84.0f));
            } else {
                ImGui::InputText("##value", buffer.data(), buffer.size());
            }
            if (ImGui::IsItemActivated()) {
                mActiveText = row.option;
            }
            if (ImGui::IsItemDeactivated() && mActiveText == row.option) {
                mActiveText.clear();
            }
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                queueEdit(row.option, buffer.data());
            }
            break;
        }
        }
        ImGui::PopID();
    };

    // One titled group of rows: label on the left, the widget on the right, one line per setting.
    // The two columns are a table, so the widgets line up across rows whatever the label lengths are.
    auto drawSection = [&](char const* titleKey, std::vector<Row> const& rows, int idBase) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.63f, 0.63f, 0.67f, 1.0f));
        ImGui::TextUnformatted(tr(locale, titleKey).c_str());
        ImGui::PopStyleColor();
        ImGui::Spacing();
        if (rows.empty()) {
            return;
        }
        float labelW = 0.0f;
        for (auto const& row : rows) {
            labelW = std::max(labelW, ImGui::CalcTextSize(tr(locale, row.label).c_str()).x);
        }
        labelW = std::min(labelW + style.ItemSpacing.x * 2.0f, ImGui::GetContentRegionAvail().x * 0.55f);
        if (ImGui::BeginTable(
                titleKey,
                2,
                ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_PadOuterX
            )) {
            ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthFixed, labelW);
            ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch);
            for (size_t i = 0; i < rows.size(); ++i) {
                Row const& row = rows[i];
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(tr(locale, row.label).c_str());
                ImGui::TableSetColumnIndex(1);
                editor(row, idBase + static_cast<int>(i));
            }
            ImGui::EndTable();
        }
        ImGui::Spacing();
    };

    // ---------- left column: settings ------------------------------------
    // Tabbed rather than one long column: a dozen stacked sections were hard to
    // scan, and the panel lines and the extras groups each belong together.
    ImGui::BeginChild("settingsList", ImVec2(0.0f, -footerH));
    // One pill of the tab list, drawn the way shadcn draws a segmented control: a quiet track, the
    // selected pill lifted with a muted fill and a near-white label.
    auto tabPill = [&](char const* label, bool active, int id) {
        ImGuiStyle const& st   = ImGui::GetStyle();
        ImVec2 const      size(ImGui::CalcTextSize(label).x + st.FramePadding.x * 2.6f, ImGui::GetFrameHeight());
        ImVec2 const      origin = ImGui::GetCursorScreenPos();

        ImGui::PushID(2000 + id);
        bool const clicked = ImGui::InvisibleButton("##tab", size);
        bool const hovered = ImGui::IsItemHovered();
        ImGui::PopID();
        if (hovered) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }

        ImDrawList* draw = ImGui::GetWindowDrawList();
        if (active) {
            draw->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), IM_COL32(0x27, 0x27, 0x2a, 255), 8.0f);
        } else if (hovered) {
            draw->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), IM_COL32(0x18, 0x18, 0x1b, 255), 8.0f);
        }
        ImVec2 const textSize = ImGui::CalcTextSize(label);
        draw->AddText(
            ImVec2(origin.x + (size.x - textSize.x) * 0.5f, origin.y + (size.y - textSize.y) * 0.5f),
            active ? IM_COL32(0xfa, 0xfa, 0xfa, 255) : IM_COL32(0xa1, 0xa1, 0xaa, 255),
            label
        );
        return clicked;
    };

    struct TabName {
        char const* key;
        int         index;
    };

    TabName const tabs[] = {
        {"General",     0},
        {"Block lines", 1},
        {"Entity info", 2},
        {"Appearance",  3},
        {"Keys",        4},
    };
    for (auto const& tab : tabs) {
        if (tab.index != 0) {
            ImGui::SameLine(0.0f, 6.0f);
        }
        if (tabPill(tr(locale, tab.key).c_str(), mTab == tab.index, tab.index)) {
            mTab = tab.index;
        }
    }
    ImGui::Spacing();
    ImGui::Spacing();
        if (mTab == 0) { // General
            drawSection(
                "General",
                {
                    {"Enabled",           "enabled",            Kind::Bool,  0.0f, 0.0f,  false, {}             },
                    {"Show overlay",      "showOverlay",        Kind::Bool,  0.0f, 0.0f,  false, {}             },
                    {"Hide in GUIs",      "hideOverlayInGui",   Kind::Bool,  0.0f, 0.0f,  false, {}             },
                    {"Max distance",      "maxDistance",        Kind::Float, 1.0f, 64.0f, false, {}             },
                    {"Interval (ticks)",  "intervalTicks",      Kind::Int,   1.0f, 40.0f, false, {}             },
                    {"Through liquids",   "passThroughLiquids", Kind::Bool,  0.0f, 0.0f,  false, {}             },
                    {"Show when empty",   "showEmpty",          Kind::Bool,  0.0f, 0.0f,  false, {}             },
                    {"Empty text",        "emptyText",          Kind::Text,  0.0f, 0.0f,  false, {}             },
                    {"Language",          "language",           Kind::Enum,  0.0f, 0.0f,  false, languageChoices},
                    {"Overlay on remote", "overlayOnRemote",    Kind::Enum,  0.0f, 0.0f,  false, {"off", "on"}  },
                    {"Extras (all)",      "extras.enabled",     Kind::Bool,  0.0f, 0.0f,  false, {}             },
            },
                0
            );
        }
        // The block panel: one switch per field, grouped by the line each feeds.
        if (mTab == 1) { // Block lines
            drawSection(
                "Title line",
                {
                    {"Name",   "display.name",   Kind::Bool, 0.0f, 0.0f, false, {}},
                    {"Facing", "display.facing", Kind::Bool, 0.0f, 0.0f, false, {}},
            },
                20
            );
            drawSection(
                "Details line",
                {
                    {"Type id",         "display.identifier",     Kind::Bool, 0.0f, 0.0f, false, {}},
                    {"Translation key", "display.translationKey", Kind::Bool, 0.0f, 0.0f, false, {}},
            },
                40
            );
            drawSection(
                "Position line",
                {
                    {"Position", "display.position", Kind::Bool, 0.0f, 0.0f, false, {}},
                    {"Distance", "display.distance", Kind::Bool, 0.0f, 0.0f, false, {}},
            },
                60
            );
            drawSection(
                "State line",
                {
                    {"Light",    "display.light",    Kind::Bool, 0.0f, 0.0f, false, {}},
                    {"Emission", "display.emission", Kind::Bool, 0.0f, 0.0f, false, {}},
            },
                80
            );
            drawSection(
                "Extras",
                {
                    {"Extras lines", "display.extras", Kind::Bool, 0.0f, 0.0f, false, {}},
            },
                100
            );
            // The block half of the extras moved in here: a block's own lines belong next to the
            // block's other lines, not in a tab of their own shared with the entity extras.
            drawSection(
                "Extras: every block",
                {
                    {"Breaking time",        "extras.hardness",        Kind::Bool, 0.0f, 0.0f, false, {}},
                    {"Explosion resistance", "extras.blastResistance", Kind::Bool, 0.0f, 0.0f, false, {}},
            },
                130
            );
            drawSection(
                "Extras: containers",
                {
                    {"Containers",      "extras.chest",            Kind::Bool, 0.0f,  0.0f, false, {} },
                    {"Item icons",      "extras.chestIcons",       Kind::Bool, 0.0f,  0.0f, false, {} },
                    {"Merge stacks",    "extras.chestIconsMerge",  Kind::Bool, 0.0f,  0.0f, false, {} },
                    {"Icons per row",   "extras.chestIconsPerRow", Kind::Int,  1.0f, 27.0f, false, {} },
                    {"Icon limit",      "extras.chestIconLimit",   Kind::Int,  0.0f, 54.0f, false, {} },
                    {"Bookshelf",       "extras.bookshelf",        Kind::Bool, 0.0f,  0.0f, false, {} },
                    {"Shelf",           "extras.shelf",            Kind::Bool, 0.0f,  0.0f, false, {} },
                    {"Lectern",         "extras.lectern",          Kind::Bool, 0.0f,  0.0f, false, {} },
                    {"Pot",             "extras.pot",              Kind::Bool, 0.0f,  0.0f, false, {} },
                    {"Brewing stands",  "extras.brewing",          Kind::Bool, 0.0f,  0.0f, false, {} },
                    {"Furnaces",        "extras.furnace",          Kind::Bool, 0.0f,  0.0f, false, {} },
            },
                140
            );
            drawSection(
                "Extras: block entities",
                {
                    {"Jukebox",    "extras.jukebox",   Kind::Bool, 0.0f, 0.0f, false, {}},
                    {"Sign",       "extras.sign",      Kind::Bool, 0.0f, 0.0f, false, {}},
                    {"Banner",     "extras.banner",    Kind::Bool, 0.0f, 0.0f, false, {}},
                    {"Item frame", "extras.itemFrame", Kind::Bool, 0.0f, 0.0f, false, {}},
                    {"Flower pot", "extras.flowerPot", Kind::Bool, 0.0f, 0.0f, false, {}},
                    {"Painting",   "extras.painting",  Kind::Bool, 0.0f, 0.0f, false, {}},
                    {"Piston",     "extras.piston",    Kind::Bool, 0.0f, 0.0f, false, {}},
            },
                160
            );
            drawSection(
                "Extras: redstone",
                {
                    {"Redstone",        "extras.redstone",      Kind::Bool, 0.0f, 0.0f, false, {}},
                    {"Repeaters",       "extras.repeater",      Kind::Bool, 0.0f, 0.0f, false, {}},
                    {"Comparators",     "extras.comparator",    Kind::Bool, 0.0f, 0.0f, false, {}},
                    {"Dispensers",      "extras.dispenser",     Kind::Bool, 0.0f, 0.0f, false, {}},
                    {"Candles",         "extras.candle",        Kind::Bool, 0.0f, 0.0f, false, {}},
                    {"Respawn anchors", "extras.respawnAnchor", Kind::Bool, 0.0f, 0.0f, false, {}},
            },
                180
            );
            drawSection(
                "Extras: block states",
                {
                    {"Block states", "extras.misc", Kind::Bool, 0.0f, 0.0f, false, {}},
            },
                200
            );
        }
        // The entity panel is a layout of its own (renderEntityText) with its own
        // switches: hit points instead of light level and emission, which describe a
        // world position rather than the entity.
        if (mTab == 2) { // Entity info
            drawSection(
                "Entity info",
                {
                    {"Entity info", "entityEnabled", Kind::Bool, 0.0f, 0.0f, false, {}},
            },
                300
            );
            drawSection(
                "Title line",
                {
                    {"Name",   "entity.name",   Kind::Bool, 0.0f, 0.0f, false, {}},
                    {"Facing", "entity.facing", Kind::Bool, 0.0f, 0.0f, false, {}},
            },
                320
            );
            drawSection(
                "Details line",
                {
                    {"Type id",         "entity.identifier",     Kind::Bool, 0.0f, 0.0f, false, {}},
                    {"Translation key", "entity.translationKey", Kind::Bool, 0.0f, 0.0f, false, {}},
            },
                340
            );
            drawSection(
                "Position line",
                {
                    {"Position", "entity.position", Kind::Bool, 0.0f, 0.0f, false, {}},
                    {"Distance", "entity.distance", Kind::Bool, 0.0f, 0.0f, false, {}},
            },
                360
            );
            drawSection(
                "State line",
                {
                    {"Health", "entity.health", Kind::Bool, 0.0f, 0.0f, false, {}},
            },
                380
            );
            drawSection(
                "Hit point sprites",
                {
                    {"Health style",
                     "entity.healthStyle",                     Kind::Enum,
                     0.0f,                                              0.0f,
                     false,                                                      {"bar",
                                                                                  "bar+number",
                                                                                  "hearts",
                                                                                  "hearts+number",
                                                                                  "number"}    },
                    {"Hearts threshold", "entity.heartsThreshold", Kind::Float, 2.0f, 200.0f, false, {}                                     },
                    {"Hearts per row",   "entity.heartsPerRow",    Kind::Int,   1.0f,  40.0f, false, {}                                     },
                    {"Health decimals",  "entity.healthDecimals",  Kind::Bool,  0.0f,   0.0f, false, {}                                     },
                    {"Armor row",        "entity.armor",           Kind::Bool,  0.0f,   0.0f, false, {}                                     },
            },
                390
            );
            drawSection(
                "Extras",
                {
                    {"Extras lines", "entity.extras", Kind::Bool, 0.0f, 0.0f, false, {}},
            },
                400
            );
            // The entity half of the extras: what an entity reports about itself belongs next to
            // the entity's other lines.
            drawSection(
                "Extras: entities",
                {
                    {"Owner",           "entityExtras.owner",        Kind::Bool, 0.0f,  0.0f, false, {}                  },
                    {"Status effects",  "entityExtras.effects",      Kind::Bool, 0.0f,  0.0f, false, {}                  },
                    {"Effect limit",    "entityExtras.effectsLimit", Kind::Int,  0.0f, 38.0f, false, {}                  },
                    {"Breeding timer",  "entityExtras.breeding",     Kind::Bool, 0.0f,  0.0f, false, {}                  },
                    {"Conversion time", "entityExtras.conversion",   Kind::Bool, 0.0f,  0.0f, false, {}                  },
                    {"Pet armor",       "entityExtras.petArmor",     Kind::Enum, 0.0f,  0.0f, false, {"all",
                                                                                                          "damaged",
                                                                                                          "off"}             },
                    {"Mount numbers",   "entityExtras.horse",        Kind::Bool, 0.0f,  0.0f, false, {}                  },
                    {"Armor stand pose","entityExtras.armorStand",   Kind::Bool, 0.0f,  0.0f, false, {}                  },
                    {"Villager trade",  "entityExtras.villager",     Kind::Bool, 0.0f,  0.0f, false, {}                  },
                    {"Waxed golem",     "entityExtras.waxed",        Kind::Bool, 0.0f,  0.0f, false, {}                  },
            },
                230
            );
        }
        // Appearance and colours used to sit in a column of their own beside the tabs. That
        // column took half the window, so the window had to be larger than the panel it
        // configures and covered it; as a tab they get the full width and the panel stays in
        // view while they are edited.
        if (mTab == 3) { // Appearance
            drawSection(
                "Appearance",
                {
                    {"Text colour",   "textColor",       Kind::Text,  0.0f, 0.0f, false, {}},
                    {"Font size",     "fontSize",        Kind::Float, 0.5f, 2.0f, false, {}},
                    {"Panel",         "background",      Kind::Bool,  0.0f, 0.0f, false, {}},
                    {"Panel opacity", "backgroundAlpha", Kind::Float, 0.0f, 1.0f, false, {}},
                    {"Text shadow",   "shadow",          Kind::Bool,  0.0f, 0.0f, false, {}},
                    {"Max width",     "maxWidth",        Kind::Float, 0.0f, 1.0f, false, {}},
                    {"Fade time",     "transitionTime",  Kind::Float, 0.0f, 1.0f, false, {}},
                    {"Offset X",      "offsetX",         Kind::Float, 0.0f, 1.0f, false, {}},
                    {"Offset Y",      "offsetY",         Kind::Float, 0.0f, 1.0f, false, {}},
                    {"Anchor",
                     "anchor",                           Kind::Enum,
                     0.0f,                                                  0.0f,
                     false,                                                              {"top_left",
                      "top_center",
                      "top_right",
                      "middle_left",
                      "center",
                      "middle_right",
                      "bottom_left",
                      "bottom_center",
                      "bottom_right"}                                   },
            },
                1000
            );

            ImGui::Spacing();

            drawSection(
                "Colours",
                {
                    {"Name colour",            "colors.name",           Kind::Enum, 0.0f, 0.0f, false, colorChoices()},
                    {"Facing colour",          "colors.facing",         Kind::Enum, 0.0f, 0.0f, false, colorChoices()},
                    {"Type id colour",         "colors.identifier",     Kind::Enum, 0.0f, 0.0f, false, colorChoices()},
                    {"Translation key colour", "colors.translationKey", Kind::Enum, 0.0f, 0.0f, false, colorChoices()},
                    {"X colour",               "colors.x",              Kind::Enum, 0.0f, 0.0f, false, colorChoices()},
                    {"Y colour",               "colors.y",              Kind::Enum, 0.0f, 0.0f, false, colorChoices()},
                    {"Z colour",               "colors.z",              Kind::Enum, 0.0f, 0.0f, false, colorChoices()},
                    {"Distance colour",        "colors.distance",       Kind::Enum, 0.0f, 0.0f, false, colorChoices()},
                    {"Label colour",           "colors.label",          Kind::Enum, 0.0f, 0.0f, false, colorChoices()},
                    {"Value colour",           "colors.value",          Kind::Enum, 0.0f, 0.0f, false, colorChoices()},
                    {"Health colour",          "colors.health",         Kind::Enum, 0.0f, 0.0f, false, colorChoices()},
            },
                1100
            );
        }
        if (mTab == 4) { // Keys
            drawSection(
                "Keys",
                {
                    {"Key: open screen", "keyOpenConfig", Kind::Key, 0.0f, 0.0f, false, {}},
                    {"Key: toggle info", "keyToggleShow", Kind::Key, 0.0f, 0.0f, false, {}},
            },
                220
            );
        }
    ImGui::EndChild();

    // ---------- footer ---------------------------------------------------
    if (!status.empty()) {
        ImVec4 const colour = statusOk ? ImVec4(0.42f, 0.85f, 0.55f, 1.0f) : ImVec4(0.96f, 0.45f, 0.42f, 1.0f);
        ImGui::TextColored(colour, "%s", status.c_str());
        ImGui::SameLine();
    }
    float const buttonW = (ImGui::CalcTextSize(tr(locale, "Reload config").c_str()).x + style.FramePadding.x * 2.0f)
                        + (ImGui::CalcTextSize(tr(locale, "Close").c_str()).x + style.FramePadding.x * 2.0f)
                        + style.ItemSpacing.x;
    ImGui::SetCursorPosX(
        ImGui::GetCursorPosX() < ImGui::GetWindowContentRegionMax().x - buttonW
            ? ImGui::GetWindowContentRegionMax().x - buttonW
            : ImGui::GetCursorPosX()
    );
    if (ImGui::Button(tr(locale, "Reload config").c_str())) {
        queueEdit("reload", "");
    }
    ImGui::SameLine();
    // the primary action, the way shadcn draws it: near-white fill, dark label
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.98f, 0.98f, 0.98f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.90f, 0.90f, 0.92f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.80f, 0.80f, 0.82f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.094f, 0.094f, 0.106f, 1.0f));
    if (ImGui::Button(tr(locale, "Close").c_str())) {
        setVisible(false);
    }
    ImGui::PopStyleColor(4);

    ImGui::End();
}

} // namespace insight
