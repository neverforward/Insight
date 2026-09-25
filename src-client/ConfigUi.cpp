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
        {"showOverlay",            &cfg.client.showOverlay     },
        {"hideOverlayInGui",       &cfg.client.hideOverlayInGui},
        {"background",             &cfg.client.background      },
        {"shadow",                 &cfg.client.shadow          },
        {"extras.enabled",         &cfg.extras.enabled         },
        {"extras.hardness",        &cfg.extras.hardness        },
        {"extras.blastResistance", &cfg.extras.blastResistance },
        {"extras.chest",           &cfg.extras.chest           },
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
    };
    for (auto const& entry : table) {
        if (option == entry.name) {
            return entry.value;
        }
    }
    return nullptr;
}

std::string currentValueText(Row const& row, std::string const& locale) {
    auto const& cfg = Insight::cfg();

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
    if (row.option == "format") {
        return cfg.format;
    }
    if (row.option == "entityFormat") {
        return cfg.entityFormat;
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

bool rowIsOn(Row const& row, std::string const& locale) { return currentValueText(row, locale) == tr(locale, "On"); }


// Theme of the configuration screen: a dark, low-contrast panel with one blue
// accent (used for the check mark, sliders, the open-row bar and the chevron).
void applyStyle() {
    ImGuiStyle& s = ImGui::GetStyle();
    ImGui::StyleColorsDark(&s);

    s.WindowRounding          = 8.0f;
    s.ChildRounding           = 6.0f;
    s.FrameRounding           = 5.0f;
    s.PopupRounding           = 6.0f;
    s.ScrollbarRounding       = 8.0f;
    s.GrabRounding            = 5.0f;
    s.WindowBorderSize        = 1.0f;
    s.ChildBorderSize         = 1.0f;
    s.FrameBorderSize         = 0.0f;
    s.WindowPadding           = ImVec2(14.0f, 12.0f);
    s.FramePadding            = ImVec2(10.0f, 6.0f);
    s.ItemSpacing             = ImVec2(10.0f, 7.0f);
    s.ItemInnerSpacing        = ImVec2(8.0f, 6.0f);
    s.IndentSpacing           = 16.0f;
    s.ScrollbarSize           = 12.0f;
    s.WindowTitleAlign        = ImVec2(0.0f, 0.5f);
    s.SeparatorTextBorderSize = 2.0f;
    s.SeparatorTextPadding    = ImVec2(0.0f, 8.0f);

    auto rgb = [](int r, int g, int b, float a = 1.0f) {
        return ImVec4(
            static_cast<float>(r) / 255.0f,
            static_cast<float>(g) / 255.0f,
            static_cast<float>(b) / 255.0f,
            a
        );
    };
    ImVec4* col                        = s.Colors;
    col[ImGuiCol_Text]                 = rgb(228, 233, 240);
    col[ImGuiCol_TextDisabled]         = rgb(138, 148, 164);
    col[ImGuiCol_WindowBg]             = rgb(19, 22, 27, 0.98f);
    col[ImGuiCol_ChildBg]              = rgb(25, 29, 35);
    col[ImGuiCol_PopupBg]              = rgb(25, 29, 35, 0.98f);
    col[ImGuiCol_Border]               = rgb(47, 54, 64);
    col[ImGuiCol_BorderShadow]         = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    col[ImGuiCol_FrameBg]              = rgb(38, 44, 52);
    col[ImGuiCol_FrameBgHovered]       = rgb(48, 56, 66);
    col[ImGuiCol_FrameBgActive]        = rgb(58, 68, 80);
    col[ImGuiCol_TitleBg]              = rgb(25, 29, 35);
    col[ImGuiCol_TitleBgActive]        = rgb(31, 37, 45);
    col[ImGuiCol_MenuBarBg]            = rgb(25, 29, 35);
    col[ImGuiCol_ScrollbarBg]          = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    col[ImGuiCol_ScrollbarGrab]        = rgb(58, 66, 78);
    col[ImGuiCol_ScrollbarGrabHovered] = rgb(72, 82, 96);
    col[ImGuiCol_ScrollbarGrabActive]  = rgb(88, 160, 255);
    col[ImGuiCol_CheckMark]            = rgb(88, 160, 255);
    col[ImGuiCol_SliderGrab]           = rgb(88, 160, 255);
    col[ImGuiCol_SliderGrabActive]     = rgb(120, 180, 255);
    col[ImGuiCol_Button]               = rgb(38, 44, 52);
    col[ImGuiCol_ButtonHovered]        = rgb(52, 62, 74);
    col[ImGuiCol_ButtonActive]         = rgb(88, 160, 255);
    col[ImGuiCol_Header]               = rgb(38, 46, 56);
    col[ImGuiCol_HeaderHovered]        = rgb(46, 56, 68);
    col[ImGuiCol_HeaderActive]         = rgb(58, 70, 84);
    col[ImGuiCol_Separator]            = rgb(47, 54, 64);
    col[ImGuiCol_SeparatorHovered]     = rgb(88, 160, 255, 0.7f);
    col[ImGuiCol_SeparatorActive]      = rgb(88, 160, 255);
    col[ImGuiCol_TextSelectedBg]       = rgb(88, 160, 255, 0.35f);
    col[ImGuiCol_TextLink]             = rgb(88, 160, 255);
    col[ImGuiCol_NavHighlight]         = rgb(88, 160, 255);
}

// The value on a row must stay on a single line: the format rows hold the whole
// multi-line format string, and drawing that verbatim pushed it over the
// neighbouring rows and out of the window.
std::string oneLineValue(std::string const& text, float maxWidth) {
    std::string out;
    out.reserve(text.size());
    bool lastWasSpace = false;
    for (char const ch : text) {
        if (ch == '\n' || ch == '\r' || ch == '\t' || ch == ' ') {
            if (!lastWasSpace && !out.empty()) {
                out.push_back(' ');
            }
            lastWasSpace = true;
            continue;
        }
        out.push_back(ch);
        lastWasSpace = false;
    }
    while (!out.empty() && out.back() == ' ') {
        out.pop_back();
    }
    if (maxWidth <= 8.0f || ImGui::CalcTextSize(out.c_str()).x <= maxWidth) {
        return out;
    }
    static char const* const dots = "...";
    while (!out.empty() && ImGui::CalcTextSize((out + dots).c_str()).x > maxWidth) {
        out.pop_back();
        // never cut a UTF-8 sequence in half
        while (!out.empty() && (static_cast<unsigned char>(out.back()) & 0xC0) == 0x80) {
            out.pop_back();
        }
    }
    return out.empty() ? std::string{} : out + dots;
}

// A clickable list row: label left, current value right, ">" affordance.
// Returns true when it was clicked (the caller then shows the inline editor).
bool rowHeader(char const* id, std::string const& label, std::string const& value, bool open) {
    ImGui::PushID(id);
    ImGuiStyle const& style   = ImGui::GetStyle();
    bool const        clicked = ImGui::Selectable(
        "##row",
        false,
        ImGuiSelectableFlags_None,
        ImVec2(0.0f, ImGui::GetFrameHeight() + style.FramePadding.y * 0.5f)
    );

    ImVec2 const min  = ImGui::GetItemRectMin();
    ImVec2 const max  = ImGui::GetItemRectMax();
    ImDrawList*  draw = ImGui::GetWindowDrawList();
    float const  pad  = style.FramePadding.x;
    float const  fh   = ImGui::GetTextLineHeight();
    float const  y    = min.y + (max.y - min.y - fh) * 0.5f;

    float const textX = min.x + pad + 2.0f;
    // accent bar on the expanded row, chevron next to the value
    if (open) {
        draw->AddRectFilled(
            ImVec2(min.x + 1.0f, min.y + 4.0f),
            ImVec2(min.x + 4.0f, max.y - 4.0f),
            ImGui::GetColorU32(ImGuiCol_CheckMark),
            2.0f
        );
    }
    char const* const arrow  = open ? "v" : ">";
    float const       arrowW = ImGui::CalcTextSize(arrow).x;
    if (!value.empty()) {
        float const       valueRight = max.x - pad - arrowW - 10.0f;
        float const       valueLeft  = textX + ImGui::CalcTextSize(label.c_str()).x + 12.0f;
        std::string const shown      = oneLineValue(value, valueRight - valueLeft);
        if (!shown.empty()) {
            float const  valueW = ImGui::CalcTextSize(shown.c_str()).x;
            ImVec4 const dim    = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
            draw->AddText(ImVec2(valueRight - valueW, y), ImGui::ColorConvertFloat4ToU32(dim), shown.c_str());
        }
    }
    ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
    accent.w      = open ? 0.95f : 0.55f;
    draw->AddText(ImVec2(max.x - pad - arrowW, y), ImGui::ColorConvertFloat4ToU32(accent), arrow);
    draw->AddText(ImVec2(textX, y), ImGui::GetColorU32(ImGuiCol_Text), label.c_str());

    ImGui::PopID();
    return clicked;
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

void ConfigUi::setPreviewText(std::string text) {
    std::lock_guard lock(mMutex);
    mPreviewText = std::move(text);
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
    static bool styleApplied = false;
    if (!styleApplied) {
        applyStyle();
        styleApplied = true;
    }

    // --- state handed over by the game thread ----------------------------
    std::vector<Event> events;
    std::string        preview;
    std::string        locale;
    std::string        status;
    bool               statusOk = true;
    {
        std::lock_guard lock(mMutex);
        events.swap(mEvents);
        preview  = mPreviewText;
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

    auto const& cfg = Insight::cfg();

    // 75% of the screen, centred; re-evaluated every frame so a resolution or
    // window-size change is followed
    ImVec2 const display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowSize(ImVec2(display.x * 0.75f, display.y * 0.75f), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, display.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::Begin(
        tr(locale, "Insight configuration").c_str(),
        nullptr,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
    );

    // the languages this mod ships messages for, plus "auto" (read once)
    std::vector<std::string> const languageChoices = availableLanguages();

    ImGuiStyle const& style   = ImGui::GetStyle();
    float const       footerH = ImGui::GetFrameHeight() + style.ItemSpacing.y * 2.0f;
    float const       paneW   = ImGui::GetContentRegionAvail().x;
    float const       leftW   = paneW * 0.5f - style.ItemSpacing.x * 0.5f;

    // inline editor of one setting; edits are only queued (the game thread owns
    // the config file)
    auto editor = [&](Row const& row, int id) {
        ImGui::PushID(id);
        ImGui::SetNextItemWidth(-1.0f);
        switch (row.kind) {
        case Kind::Bool: {
            bool value = rowIsOn(row, locale);
            if (ImGui::Checkbox("##value", &value)) {
                queueEdit(row.option, value ? "true" : "false");
            }
            break;
        }
        case Kind::Float:
        case Kind::Int: {
            // the value is loaded when the row opens and kept while it stays
            // open: taking it from the config every frame would cancel the drag
            bool const isInt = row.kind == Kind::Int;
            if (mSliderRow != row.option) {
                mSliderRow   = row.option;
                mSliderValue = static_cast<float>(std::atof(currentValueText(row, locale).c_str()));
            }
            ImGui::SliderFloat("##value", &mSliderValue, row.lo, row.hi, isInt ? "%.0f" : "%.2f");
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                queueEdit(
                    row.option,
                    isInt ? std::to_string(static_cast<int>(mSliderValue)) : util::trimNumber(mSliderValue, 2)
                );
            }
            break;
        }
        case Kind::Enum: {
            std::string const current = currentValueText(row, locale);
            if (ImGui::BeginCombo("##value", choiceLabel(current, locale).c_str())) {
                for (auto const& choice : row.choices) {
                    if (ImGui::Selectable(choiceLabel(choice, locale).c_str(), choice == current)) {
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
            // one persistent buffer per editing session: refilling it from the
            // config every frame would fight ImGui's own edit state and make the
            // field unusable after the first change. Closing the row clears the
            // session, so reopening it always shows the current config value.
            if (mEditRow != row.option) {
                mEditRow = row.option;
                std::snprintf(mEditBuffer, sizeof(mEditBuffer), "%s", currentValueText(row, locale).c_str());
            }
            if (row.multiline) {
                ImGui::InputTextMultiline("##value", mEditBuffer, sizeof(mEditBuffer), ImVec2(-1.0f, 84.0f));
            } else {
                ImGui::InputText("##value", mEditBuffer, sizeof(mEditBuffer));
            }
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                queueEdit(row.option, mEditBuffer);
            }
            break;
        }
        }
        ImGui::PopID();
    };

    // one titled group of rows
    auto drawSection = [&](char const* titleKey, std::vector<Row> const& rows, int idBase) {
        ImGui::SeparatorText(tr(locale, titleKey).c_str());
        for (size_t i = 0; i < rows.size(); ++i) {
            Row const& row  = rows[i];
            bool const open = mOpenRow == row.option;
            if (rowHeader(
                    row.option.c_str(),
                    tr(locale, row.label),
                    choiceLabel(currentValueText(row, locale), locale),
                    open
                )) {
                mOpenRow = open ? std::string{} : row.option;
            }
            if (open) {
                ImGui::Indent();
                editor(row, idBase + static_cast<int>(i));
                ImGui::Unindent();
                ImGui::Spacing();
            } else {
                // closing the row ends its editing session: reopening it loads the
                // current values from the config again
                if (mEditRow == row.option) {
                    mEditRow.clear();
                }
                if (mSliderRow == row.option) {
                    mSliderRow.clear();
                }
            }
        }
        ImGui::Spacing();
    };

    // ---------- left column: settings ------------------------------------
    ImGui::BeginGroup();
    ImGui::BeginChild("settingsList", ImVec2(leftW, -footerH));
    drawSection(
        "Info display",
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
            {"Entity info",       "entityEnabled",      Kind::Bool,  0.0f, 0.0f,  false, {}             },
            {"Entity format",     "entityFormat",       Kind::Text,  0.0f, 0.0f,  true,  {}             },
            {"Block format",      "format",             Kind::Text,  0.0f, 0.0f,  true,  {}             },
    },
        0
    );
    drawSection(
        "Extras",
        {
            {"Extras (all)",         "extras.enabled",         Kind::Bool, 0.0f, 0.0f, false, {}},
            {"Breaking time",        "extras.hardness",        Kind::Bool, 0.0f, 0.0f, false, {}},
            {"Explosion resistance", "extras.blastResistance", Kind::Bool, 0.0f, 0.0f, false, {}},
    },
        100
    );
    drawSection(
        "Containers",
        {
            {"Containers",     "extras.chest",     Kind::Bool, 0.0f, 0.0f, false, {}},
            {"Bookshelf",      "extras.bookshelf", Kind::Bool, 0.0f, 0.0f, false, {}},
            {"Shelf",          "extras.shelf",     Kind::Bool, 0.0f, 0.0f, false, {}},
            {"Lectern",        "extras.lectern",   Kind::Bool, 0.0f, 0.0f, false, {}},
            {"Pot",            "extras.pot",       Kind::Bool, 0.0f, 0.0f, false, {}},
            {"Brewing stands", "extras.brewing",   Kind::Bool, 0.0f, 0.0f, false, {}},
            {"Furnaces",       "extras.furnace",   Kind::Bool, 0.0f, 0.0f, false, {}},
    },
        120
    );
    drawSection(
        "Block entities",
        {
            {"Jukebox",    "extras.jukebox",   Kind::Bool, 0.0f, 0.0f, false, {}},
            {"Sign",       "extras.sign",      Kind::Bool, 0.0f, 0.0f, false, {}},
            {"Banner",     "extras.banner",    Kind::Bool, 0.0f, 0.0f, false, {}},
            {"Item frame", "extras.itemFrame", Kind::Bool, 0.0f, 0.0f, false, {}},
            {"Flower pot", "extras.flowerPot", Kind::Bool, 0.0f, 0.0f, false, {}},
            {"Painting",   "extras.painting",  Kind::Bool, 0.0f, 0.0f, false, {}},
            {"Piston",     "extras.piston",    Kind::Bool, 0.0f, 0.0f, false, {}},
    },
        140
    );
    drawSection(
        "Redstone",
        {
            {"Redstone",        "extras.redstone",      Kind::Bool, 0.0f, 0.0f, false, {}},
            {"Repeaters",       "extras.repeater",      Kind::Bool, 0.0f, 0.0f, false, {}},
            {"Comparators",     "extras.comparator",    Kind::Bool, 0.0f, 0.0f, false, {}},
            {"Dispensers",      "extras.dispenser",     Kind::Bool, 0.0f, 0.0f, false, {}},
            {"Candles",         "extras.candle",        Kind::Bool, 0.0f, 0.0f, false, {}},
            {"Respawn anchors", "extras.respawnAnchor", Kind::Bool, 0.0f, 0.0f, false, {}},
    },
        160
    );
    drawSection(
        "Block states",
        {
            {"Block states", "extras.misc", Kind::Bool, 0.0f, 0.0f, false, {}},
    },
        180
    );
    drawSection(
        "Keys",
        {
            {"Key: open screen", "keyOpenConfig", Kind::Key, 0.0f, 0.0f, false, {}},
            {"Key: toggle info", "keyToggleShow", Kind::Key, 0.0f, 0.0f, false, {}},
    },
        200
    );
    ImGui::EndChild();
    ImGui::EndGroup();

    ImGui::SameLine();

    // ---------- right column: preview + appearance ------------------------
    ImGui::BeginGroup();
    ImGui::BeginChild("rightPane", ImVec2(0.0f, -footerH));

    ImGui::SeparatorText(tr(locale, "Preview").c_str());
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.055f, 0.070f, 0.090f, 1.0f));
    ImGui::BeginChild("preview", ImVec2(-1.0f, ImGui::GetContentRegionAvail().y * 0.45f), ImGuiChildFlags_Borders);
    {
        // The preview *is* the info display: the overlay draws the content it is
        // currently showing, with the same colour runs, shadow and font (only
        // scaled by the configured font size), so what you see here is exactly
        // what the panel renders.
        ImVec2 const origin = ImGui::GetCursorScreenPos();
        ImVec2 const avail  = ImGui::GetContentRegionAvail();
        if (preview.empty()) {
            ImGui::TextDisabled("%s", tr(locale, "Nothing under the crosshair").c_str());
        } else {
            // the preview area stands in for the screen: the panel is placed in it
            // with the same anchor/offset rules, so those rows show their effect
            ImGuiOverlay::drawContentPreview(
                origin.x,
                origin.y,
                avail.x,
                avail.y,
                std::clamp(cfg.client.fontSize, 0.5f, 2.5f),
                cfg.client.anchor,
                cfg.client.offsetX,
                cfg.client.offsetY
            );
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();
    drawSection(
        "Appearance",
        {
            {"Text colour",   "textColor",       Kind::Text,  0.0f, 0.0f, false, {}},
            {"Font size",     "fontSize",        Kind::Float, 0.5f, 2.0f, false, {}},
            {"Panel",         "background",      Kind::Bool,  0.0f, 0.0f, false, {}},
            {"Panel opacity", "backgroundAlpha", Kind::Float, 0.0f, 1.0f, false, {}},
            {"Text shadow",   "shadow",          Kind::Bool,  0.0f, 0.0f, false, {}},
            {"Max width",     "maxWidth",        Kind::Float, 0.0f, 1.0f, false, {}},
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

    ImGui::EndChild();
    ImGui::EndGroup();

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
    if (ImGui::Button(tr(locale, "Close").c_str())) {
        setVisible(false);
    }

    ImGui::End();
}

} // namespace insight
