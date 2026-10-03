#include "ConfigGui.h"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "ll/api/form/CustomForm.h"
#include "ll/api/form/SimpleForm.h"

#include "Colors.h"
#include "Config.h"
#include "I18n.h"
#include "Insight.h"
#include "Util.h"

namespace insight::gui {

namespace {

// What kind of control an option needs. Text is the odd one out: only the free text
// option (the empty-target message) uses it. Color behaves like Enum but offers the
// formatting codes instead of a fixed list of its own.
enum class Kind { Bool, Number, Text, Enum, Color, Count };

// One row of the menu. `get` reads the value out of the live configuration and hands it
// back in exactly the form `/insight set` accepts, so the form shows what the file holds
// and the write-back goes through the command's own path.
struct Option {
    char const*              name;  // the name applyConfigEdit() takes
    char const*              label; // message key shown to the player
    Kind                     kind = Kind::Bool;
    double                   lo   = 0.0;
    double                   hi   = 1.0;
    double                   step = 1.0;
    std::vector<std::string> choices; // Enum only
    std::string (*get)() = nullptr;   // the current value as text
};

std::string boolText(bool value) { return value ? "true" : "false"; }

// A fixed choice is stored as the token the config file uses ("actionbar"); the dropdown
// shows the localized name where one exists.
std::string choiceLabel(std::string const& value, std::string const& locale) {
    static constexpr std::pair<char const*, char const*> kChoices[] = {
        {"bar",           "Bar"          },
        {"bar+number",    "Bar + number" },
        {"hearts",        "Hearts"       },
        {"hearts+number", "Hearts + number"},
        {"number",        "Number"       },
    };
    for (auto const& [token, key] : kChoices) {
        if (value == token) {
            return tr(locale, key);
        }
    }
    return value;
}

using Group = std::pair<char const*, std::vector<Option>>;

// The groups the menu offers. Every option the command accepts is in here except the
// client-only ones; .cache/check_gui_options.py keeps that claim honest.
std::vector<Group> const& groups() {
    static std::vector<Group> const table = {
        {"General",
         {
             {"enabled", "Enabled", Kind::Bool, 0, 0, 0, {}, [] { return boolText(Insight::cfg().enabled); }},
             {"enabledByDefault",
              "Enabled by default",
              Kind::Bool,
              0,
              0,
              0,
              {},
              [] { return boolText(Insight::cfg().enabledByDefault); }},
             {"maxDistance",
              "Max distance",
              Kind::Number,
              1,
              256,
              1,
              {},
              [] { return util::trimNumber(Insight::cfg().maxDistance, 1); }},
             {"intervalTicks",
              "Interval (ticks)",
              Kind::Number,
              1,
              200,
              1,
              {},
              [] { return std::to_string(Insight::cfg().intervalTicks); }},
             {"passThroughLiquids",
              "Through liquids",
              Kind::Bool,
              0,
              0,
              0,
              {},
              [] { return boolText(Insight::cfg().passThroughLiquids); }},
             {"showEmpty", "Show when empty", Kind::Bool, 0, 0, 0, {}, [] { return boolText(Insight::cfg().showEmpty); }},
             {"emptyText", "Empty text", Kind::Text, 0, 0, 0, {}, [] { return Insight::cfg().emptyText; }},
             {"entityEnabled",
              "Entity info",
              Kind::Bool,
              0,
              0,
              0,
              {},
              [] { return boolText(Insight::cfg().entityEnabled); }},
             {"channel",
              "Channel",
              Kind::Enum,
              0,
              0,
              0,
              {"none", "actionbar", "tip", "popup", "jukebox", "system", "chat"},
              [] { return Insight::cfg().server.channel; }},
         }},
        {"Block lines",
         {
             {"display.name", "Name", Kind::Bool, 0, 0, 0, {}, [] { return boolText(Insight::cfg().display.name); }},
             {"display.facing", "Facing", Kind::Bool, 0, 0, 0, {}, [] { return boolText(Insight::cfg().display.facing); }},
             {"display.identifier",
              "Type id",
              Kind::Bool,
              0,
              0,
              0,
              {},
              [] { return boolText(Insight::cfg().display.identifier); }},
             {"display.translationKey",
              "Translation key",
              Kind::Bool,
              0,
              0,
              0,
              {},
              [] { return boolText(Insight::cfg().display.translationKey); }},
             {"display.position",
              "Position",
              Kind::Bool,
              0,
              0,
              0,
              {},
              [] { return boolText(Insight::cfg().display.position); }},
             {"display.distance",
              "Distance",
              Kind::Bool,
              0,
              0,
              0,
              {},
              [] { return boolText(Insight::cfg().display.distance); }},
             {"display.light", "Light", Kind::Bool, 0, 0, 0, {}, [] { return boolText(Insight::cfg().display.light); }},
             {"display.emission",
              "Emission",
              Kind::Bool,
              0,
              0,
              0,
              {},
              [] { return boolText(Insight::cfg().display.emission); }},
             {"display.extras",
              "Extras lines",
              Kind::Bool,
              0,
              0,
              0,
              {},
              [] { return boolText(Insight::cfg().display.extras); }},
         }},
        {"Entity lines",
         {
             {"entity.name", "Name", Kind::Bool, 0, 0, 0, {}, [] { return boolText(Insight::cfg().entity.name); }},
             {"entity.facing", "Facing", Kind::Bool, 0, 0, 0, {}, [] { return boolText(Insight::cfg().entity.facing); }},
             {"entity.identifier",
              "Type id",
              Kind::Bool,
              0,
              0,
              0,
              {},
              [] { return boolText(Insight::cfg().entity.identifier); }},
             {"entity.translationKey",
              "Translation key",
              Kind::Bool,
              0,
              0,
              0,
              {},
              [] { return boolText(Insight::cfg().entity.translationKey); }},
             {"entity.position",
              "Position",
              Kind::Bool,
              0,
              0,
              0,
              {},
              [] { return boolText(Insight::cfg().entity.position); }},
             {"entity.distance",
              "Distance",
              Kind::Bool,
              0,
              0,
              0,
              {},
              [] { return boolText(Insight::cfg().entity.distance); }},
             {"entity.health", "Health", Kind::Bool, 0, 0, 0, {}, [] { return boolText(Insight::cfg().entity.health); }},
             {"entity.healthStyle",
              "Health style",
              Kind::Enum,
              0,
              0,
              0,
              {"bar", "bar+number", "hearts", "hearts+number", "number"},
              [] { return Insight::cfg().entity.healthStyle; }},
             {"entity.heartsThreshold",
              "Hearts threshold",
              Kind::Number,
              2,
              200,
              1,
              {},
              [] { return util::trimNumber(Insight::cfg().entity.heartsThreshold, 1); }},
             {"entity.heartsPerRow",
              "Hearts per row",
              Kind::Number,
              1,
              40,
              1,
              {},
              [] { return std::to_string(Insight::cfg().entity.heartsPerRow); }},
             {"entity.healthDecimals",
              "Health decimals",
              Kind::Bool,
              0,
              0,
              0,
              {},
              [] { return boolText(Insight::cfg().entity.healthDecimals); }},
             {"entity.armor",
              "Armor row",
              Kind::Bool,
              0,
              0,
              0,
              {},
              [] { return boolText(Insight::cfg().entity.armor); }},
             {"entity.extras",
              "Extras lines",
              Kind::Bool,
              0,
              0,
              0,
              {},
              [] { return boolText(Insight::cfg().entity.extras); }},
         }},
        {"Colours",
         {
             {"colors.name", "Name colour", Kind::Color, 0, 0, 0, {}, [] { return Insight::cfg().colors.name; }},
             {"colors.facing", "Facing colour", Kind::Color, 0, 0, 0, {}, [] { return Insight::cfg().colors.facing; }},
             {"colors.identifier",
              "Type id colour",
              Kind::Color,
              0,
              0,
              0,
              {},
              [] { return Insight::cfg().colors.identifier; }},
             {"colors.translationKey",
              "Translation key colour",
              Kind::Color,
              0,
              0,
              0,
              {},
              [] { return Insight::cfg().colors.translationKey; }},
             {"colors.x", "X colour", Kind::Color, 0, 0, 0, {}, [] { return Insight::cfg().colors.x; }},
             {"colors.y", "Y colour", Kind::Color, 0, 0, 0, {}, [] { return Insight::cfg().colors.y; }},
             {"colors.z", "Z colour", Kind::Color, 0, 0, 0, {}, [] { return Insight::cfg().colors.z; }},
             {"colors.distance",
              "Distance colour",
              Kind::Color,
              0,
              0,
              0,
              {},
              [] { return Insight::cfg().colors.distance; }},
             {"colors.label", "Label colour", Kind::Color, 0, 0, 0, {}, [] { return Insight::cfg().colors.label; }},
             {"colors.value", "Value colour", Kind::Color, 0, 0, 0, {}, [] { return Insight::cfg().colors.value; }},
             {"colors.health", "Health colour", Kind::Color, 0, 0, 0, {}, [] { return Insight::cfg().colors.health; }},
         }},
        {"Extras: every block",
         {
             {"extras.enabled",
              "Extras (all)",
              Kind::Bool,
              0,
              0,
              0,
              {},
              [] { return boolText(Insight::cfg().extras.enabled); }},
             {"extras.hardness",
              "Breaking time",
              Kind::Bool,
              0,
              0,
              0,
              {},
              [] { return boolText(Insight::cfg().extras.hardness); }},
             {"extras.blastResistance",
              "Explosion resistance",
              Kind::Bool,
              0,
              0,
              0,
              {},
              [] { return boolText(Insight::cfg().extras.blastResistance); }},
             {"extras.chest", "Containers", Kind::Bool, 0, 0, 0, {}, [] { return boolText(Insight::cfg().extras.chest); }},
             {"extras.bookshelf",
              "Bookshelf",
              Kind::Bool,
              0,
              0,
              0,
              {},
              [] { return boolText(Insight::cfg().extras.bookshelf); }},
             {"extras.shelf", "Shelf", Kind::Bool, 0, 0, 0, {}, [] { return boolText(Insight::cfg().extras.shelf); }},
             {"extras.lectern", "Lectern", Kind::Bool, 0, 0, 0, {}, [] { return boolText(Insight::cfg().extras.lectern); }},
             {"extras.pot", "Pot", Kind::Bool, 0, 0, 0, {}, [] { return boolText(Insight::cfg().extras.pot); }},
             {"extras.brewing",
              "Brewing stands",
              Kind::Bool,
              0,
              0,
              0,
              {},
              [] { return boolText(Insight::cfg().extras.brewing); }},
             {"extras.furnace",
              "Furnaces",
              Kind::Bool,
              0,
              0,
              0,
              {},
              [] { return boolText(Insight::cfg().extras.furnace); }},
         }},
        {"Extras: block entities",
         {
             {"extras.jukebox", "Jukebox", Kind::Bool, 0, 0, 0, {}, [] { return boolText(Insight::cfg().extras.jukebox); }},
             {"extras.sign", "Sign", Kind::Bool, 0, 0, 0, {}, [] { return boolText(Insight::cfg().extras.sign); }},
             {"extras.banner", "Banner", Kind::Bool, 0, 0, 0, {}, [] { return boolText(Insight::cfg().extras.banner); }},
             {"extras.itemFrame",
              "Item frame",
              Kind::Bool,
              0,
              0,
              0,
              {},
              [] { return boolText(Insight::cfg().extras.itemFrame); }},
             {"extras.flowerPot",
              "Flower pot",
              Kind::Bool,
              0,
              0,
              0,
              {},
              [] { return boolText(Insight::cfg().extras.flowerPot); }},
             {"extras.painting", "Painting", Kind::Bool, 0, 0, 0, {}, [] { return boolText(Insight::cfg().extras.painting); }},
             {"extras.piston", "Piston", Kind::Bool, 0, 0, 0, {}, [] { return boolText(Insight::cfg().extras.piston); }},
         }},
        {"Extras: redstone",
         {
             {"extras.redstone", "Redstone", Kind::Bool, 0, 0, 0, {}, [] { return boolText(Insight::cfg().extras.redstone); }},
             {"extras.repeater", "Repeaters", Kind::Bool, 0, 0, 0, {}, [] { return boolText(Insight::cfg().extras.repeater); }},
             {"extras.comparator",
              "Comparators",
              Kind::Bool,
              0,
              0,
              0,
              {},
              [] { return boolText(Insight::cfg().extras.comparator); }},
             {"extras.dispenser",
              "Dispensers",
              Kind::Bool,
              0,
              0,
              0,
              {},
              [] { return boolText(Insight::cfg().extras.dispenser); }},
             {"extras.candle", "Candles", Kind::Bool, 0, 0, 0, {}, [] { return boolText(Insight::cfg().extras.candle); }},
             {"extras.respawnAnchor",
              "Respawn anchors",
              Kind::Bool,
              0,
              0,
              0,
              {},
              [] { return boolText(Insight::cfg().extras.respawnAnchor); }},
             {"extras.misc", "Block states", Kind::Bool, 0, 0, 0, {}, [] { return boolText(Insight::cfg().extras.misc); }},
         }},
    };
    return table;
}

Option const* findOption(std::string const& name) {
    for (auto const& group : groups()) {
        for (auto const& option : group.second) {
            if (name == option.name) {
                return &option;
            }
        }
    }
    return nullptr;
}

// The colours a colour row offers: "none" plus every code this mod accepts, in the same
// order the client's screen lists them.
std::vector<std::string> const& colorChoices() {
    static std::vector<std::string> const choices = [] {
        std::vector<std::string> out{"none"};
        for (char const code : colorCodeChoices()) {
            out.emplace_back(1, code);
        }
        return out;
    }();
    return choices;
}

void showGroup(Player& player, Group const& group);

// The group picker: one button per group, plus a reload of the file it just wrote.
void showGroupList(Player& player) {
    auto const locale = player.getLocaleCode();

    ll::form::SimpleForm menu(tr(locale, "Insight configuration"));
    for (auto const& group : groups()) {
        // `group` lives in the static table, so the callback may hold a reference to it
        menu.appendButton(tr(locale, group.first), [&group](Player& p) { showGroup(p, group); });
    }
    menu.appendDivider();
    menu.appendButton(tr(locale, "Reload config"), [](Player& p) {
        Insight::reloadConfigFromDisk();
        p.sendMessage(tr(p.getLocaleCode(), "Insight configuration reloaded."));
    });
    menu.sendTo(player);
}

// One group as a form: every control carries the value the config holds, and the submit
// callback hands what came back to applyConfigEdit(), which validates, applies and saves.
void showGroup(Player& player, Group const& group) {
    auto const locale = player.getLocaleCode();

    ll::form::CustomForm form;
    form.setTitle(tr(locale, "Insight configuration") + " - " + tr(locale, group.first));
    form.appendHeader(tr(locale, group.first));

    for (auto const& option : group.second) {
        auto const label   = tr(locale, option.label);
        auto const current = option.get != nullptr ? option.get() : std::string{};
        switch (option.kind) {
        case Kind::Bool:
            form.appendToggle(option.name, label, current == "true");
            break;
        case Kind::Number:
            form.appendSlider(option.name, label, option.lo, option.hi, option.step, std::atof(current.c_str()));
            break;
        case Kind::Enum:
        case Kind::Color: {
            auto const& values = option.kind == Kind::Color ? colorChoices() : option.choices;
            size_t      index  = 0;
            for (size_t i = 0; i < values.size(); ++i) {
                if (values[i] == current) {
                    index = i;
                    break;
                }
            }
            std::vector<std::string> labels;
            labels.reserve(values.size());
            for (auto const& value : values) {
                labels.push_back(
                    option.kind == Kind::Color ? colorDisplayName(value, locale) : choiceLabel(value, locale)
                );
            }
            form.appendDropdown(option.name, label, labels, index);
            break;
        }
        case Kind::Text:
            form.appendInput(option.name, label, {}, current);
            break;
        case Kind::Count:
            break; // never a row of its own; keeps the switch exhaustive
        }
    }

    form.setSubmitButton(tr(locale, "Apply"));
    form.sendTo(player, [locale](Player& p, ll::form::CustomFormResult const& data, ll::form::FormCancelReason) {
        // A closed form leaves the configuration untouched, and the group list is only
        // reopened after a submit - so closing the form is always a way out.
        if (!data) {
            return;
        }
        int applied = 0;
        for (auto const& [name, value] : *data) {
            auto const* option = findOption(name);
            if (option == nullptr) {
                continue;
            }
            // A toggle and a dropdown come back as an index, a slider as a number and an
            // input as text, so each kind is turned back into the value the command takes.
            std::string text;
            if (auto const* index = std::get_if<uint64>(&value)) {
                if (option->kind == Kind::Bool) {
                    text = *index != 0 ? "true" : "false";
                } else if (option->kind == Kind::Enum || option->kind == Kind::Color) {
                    auto const& values = option->kind == Kind::Color ? colorChoices() : option->choices;
                    if (*index >= values.size()) {
                        continue;
                    }
                    text = values[static_cast<size_t>(*index)];
                } else {
                    text = std::to_string(*index);
                }
            } else if (auto const* number = std::get_if<double>(&value)) {
                text = util::trimNumber(*number, 2);
            } else if (auto const* line = std::get_if<std::string>(&value)) {
                text = *line;
            } else {
                continue;
            }
            if (auto const result = Insight::applyConfigEdit(name, text, locale); result.ok) {
                ++applied;
            } else {
                p.sendMessage(result.message); // already localized, says what was wrong
            }
        }
        p.sendMessage(tr(locale, "Insight configuration saved.") + " (" + std::to_string(applied) + ")");
        showGroupList(p);
    });
}

} // namespace

void showMenu(Player& player) { showGroupList(player); }

} // namespace insight::gui
