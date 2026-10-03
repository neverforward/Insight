#include "Insight.h"

#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>

#include "ll/api/Config.h"
#include "ll/api/i18n/I18n.h"
#include "ll/api/mod/RegisterHelper.h"
#ifdef INSIGHT_TARGET_SERVER
#include "ll/api/data/KeyValueDB.h"
#endif

#include "Colors.h"
#include "I18n.h"
#include "Util.h"

#ifdef INSIGHT_TARGET_SERVER
#include "ServerLogic.h"
#else
#include "ClientLogic.h"
#endif

namespace insight {

static Config gConfig;

namespace {

#ifdef INSIGHT_TARGET_SERVER
// Per-player data (the /insight on|off override) is kept in a LeviLamina
// key-value database so that it survives server restarts. It is created once in
// Insight::load(); ll::data::KeyValueDB is move-only, hence the unique_ptr.
std::unique_ptr<ll::data::KeyValueDB> playerDb;
#endif

} // namespace

// thread-safety: every reader of gConfig runs on the game thread (tick /
// render / command handling), and reloadConfigFromDisk / applyConfigEdit are
// also only invoked from the command handler, so no locking is required.

Config const& Insight::cfg() { return gConfig; }

std::filesystem::path Insight::configPath() { return Insight::getInstance().getSelf().getConfigDir() / "config.json"; }

std::optional<bool> Insight::playerOverride(std::string const& uuid) {
#ifdef INSIGHT_TARGET_SERVER
    if (!playerDb || uuid.empty()) {
        return std::nullopt;
    }
    auto value = playerDb->get(uuid);
    if (!value || value->empty()) {
        return std::nullopt;
    }
    return *value != "0";
#else
    (void)uuid;
    return std::nullopt;
#endif
}

void Insight::setPlayerOverride(std::string const& uuid, std::optional<bool> enabled) {
#ifdef INSIGHT_TARGET_SERVER
    if (!playerDb || uuid.empty()) {
        return;
    }
    if (enabled) {
        playerDb->set(uuid, *enabled ? "1" : "0");
    } else {
        playerDb->del(uuid);
    }
#else
    (void)uuid;
    (void)enabled;
#endif
}

namespace {

// Loads config.json. LeviLamina merges the current defaults into the file
// whenever its "version" differs from Config::version, so options added later
// appear automatically; values already in the file are kept (see Config.h).
bool loadConfigFile(Config& cfg) {
    auto path = Insight::configPath();
    return ll::config::loadConfig(cfg, path);
}

void saveConfigFile() {
    if (!ll::config::saveConfig(gConfig, Insight::configPath())) {
        Insight::getInstance().getSelf().getLogger().error(
            "Cannot save configurations to {}",
            Insight::configPath().string()
        );
    }
}

// The whole configuration file as text, or empty when it cannot be read. Used by the
// two hand-written upgrades below, before ll::config gets its hands on the file.
std::string readTextFile(std::filesystem::path const& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// --- hand-written shape upgrade ----------------------------------------------
// ll::config deserializes the whole struct in one go, so a file written before a
// group of settings existed fails to load entirely ("missing required field ...")
// and the mod falls back to the defaults - losing every value the player had. This
// runs before the file is read and adds only what is absent, taken straight from the
// current defaults: a value that is there always wins, a missing group (or a missing
// key inside one) is inserted with its default.
//
// The "version" is deliberately left alone. It is what makes ll::config merge by
// itself; this is the upgrade for a file that already claims the current version but
// has an older shape, which that mechanism cannot see.
bool fillMissingGroups(std::filesystem::path const& path) {
    std::error_code error;
    if (!std::filesystem::exists(path, error)) {
        return false;
    }
    std::string const content = readTextFile(path);
    if (content.empty()) {
        return false;
    }
    nlohmann::ordered_json fileJson;
    try {
        fileJson = nlohmann::ordered_json::parse(content, nullptr, true, true);
    } catch (...) {
        return false; // a broken file is left to the normal error path
    }
    if (!fileJson.is_object()) {
        return false;
    }
    auto defaults = ll::reflection::serialize<nlohmann::ordered_json>(Config{});
    if (!defaults) {
        return false;
    }

    auto fill = [](auto&& self, nlohmann::ordered_json& target, nlohmann::ordered_json const& source) -> bool {
        if (!target.is_object()) {
            return false;
        }
        bool changed = false;
        for (auto const& [key, value] : source.items()) {
            if (!target.contains(key)) {
                target[key] = value;
                changed     = true;
            } else if (value.is_object()) {
                changed = self(self, target[key], value) || changed;
            }
        }
        return changed;
    };
    if (!fill(fill, fileJson, *defaults)) {
        return false;
    }

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }
    out << fileJson.dump(4);
    return true;
}

// --- legacy display migration ------------------------------------------------
// A configuration written by version <= 4 rendered the panel from a text template
// ("format" / "entityFormat"; the per-block-type "overrides" are simply dropped).
// Version 5 has one switch per field instead, so the templates are read once and
// turned into switches: a placeholder the old template used means that field stays
// visible, one it never used is switched off. The panel a player had before the
// upgrade is what they keep.
//
// The raw file is scanned before ll::config merges the current defaults into it and
// rewrites it, and the migration only runs for a file that still carries an older
// version - running it on every load would undo the switches the player has changed
// since the upgrade.

// The string value of `"key": "..."` in a JSON document; empty when the key is not
// there. Escape sequences are kept verbatim - only the placeholder text inside the
// value is ever inspected.
std::string rawJsonString(std::string const& text, std::string const& key) {
    auto const at = text.find("\"" + key + "\"");
    if (at == std::string::npos) {
        return {};
    }
    auto const colon = text.find(':', at + key.size() + 2);
    if (colon == std::string::npos) {
        return {};
    }
    auto const open = text.find('"', colon + 1);
    if (open == std::string::npos) {
        return {};
    }
    std::string value;
    for (size_t i = open + 1; i < text.size(); ++i) {
        if (text[i] == '\\' && i + 1 < text.size()) {
            value += text[i];
            value += text[++i];
            continue;
        }
        if (text[i] == '"') {
            break;
        }
        value += text[i];
    }
    return value;
}

// The integer value of `"key": 12`, or `fallback` when it cannot be read.
long rawJsonInt(std::string const& text, std::string const& key, long fallback) {
    auto const at = text.find("\"" + key + "\"");
    if (at == std::string::npos) {
        return fallback;
    }
    auto const colon = text.find(':', at + key.size() + 2);
    if (colon == std::string::npos) {
        return fallback;
    }
    try {
        return std::stol(text.substr(colon + 1));
    } catch (...) {
        return fallback;
    }
}

// Turns the display templates of a version <= 4 configuration into switches.
// Returns false when the file has no templates, i.e. when there is nothing to do.
//
// A placeholder the old template used means that field stayed visible, one it never
// used is switched off. The block template feeds the block switches (display.*) and
// the entity template the entity ones (entity.*); a template that is not in the file
// at all leaves its group at the defaults instead of switching everything off.
bool migrateLegacyDisplay(std::string const& fileText, Config& cfg) {
    std::string const blockFormat  = rawJsonString(fileText, "format");
    std::string const entityFormat = rawJsonString(fileText, "entityFormat");
    if (blockFormat.empty() && entityFormat.empty()) {
        return false;
    }
    auto anyUsed = [](std::string const& text, std::initializer_list<char const*> placeholders) {
        for (auto const* placeholder : placeholders) {
            if (text.find(placeholder) != std::string::npos) {
                return true;
            }
        }
        return false;
    };

    if (!blockFormat.empty()) {
        auto& d          = cfg.display;
        d.name           = anyUsed(blockFormat, {"{blockName}"});
        d.facing         = anyUsed(blockFormat, {"{direction}"});
        d.identifier     = anyUsed(blockFormat, {"{blockType}"});
        d.translationKey = anyUsed(blockFormat, {"{blockKey}"});
        d.position       = anyUsed(blockFormat, {"{x}", "{y}", "{z}"});
        d.distance       = anyUsed(blockFormat, {"{dist}"});
        d.light          = anyUsed(blockFormat, {"{light}"});
        d.emission       = anyUsed(blockFormat, {"{emission}"});
        d.extras         = anyUsed(blockFormat, {"{extras}"});
    }
    if (!entityFormat.empty()) {
        auto& e   = cfg.entity;
        e.name    = anyUsed(entityFormat, {"{entityName}"});
        e.facing  = anyUsed(entityFormat, {"{direction}"});
        e.identifier = anyUsed(entityFormat, {"{entityType}"});
        e.position   = anyUsed(entityFormat, {"{x}", "{y}", "{z}"});
        e.distance   = anyUsed(entityFormat, {"{dist}"});
        e.health     = anyUsed(entityFormat, {"{health}", "{maxHealth}"});
        e.extras     = anyUsed(entityFormat, {"{extras}"});
        // the old entity template had no key placeholder, so entity.translationKey
        // stays at its default
    }
    return true;
}

bool parseBool(std::string const& value, bool& out) {
    auto v = util::toLower(value);
    if (v == "true" || v == "1" || v == "yes" || v == "on") {
        out = true;
        return true;
    }
    if (v == "false"
        || v

               == "0"
        || v == "no" || v == "off") {
        out = false;
        return true;
    }
    return false;
}

bool parseDouble(std::string const& value, double& out) {
    try {
        size_t used = 0;
        out         = std::stod(value, &used);
        return used == value.size();
    } catch (...) {
        return false;
    }
}

bool parseInt(std::string const& value, long& out) {
    // the configuration screen sends slider values formatted as decimals
    // ("12.00"), so any integral number is accepted, not only a bare literal
    double parsed = 0.0;
    if (!parseDouble(value, parsed) || parsed != static_cast<double>(static_cast<long>(parsed))) {
        return false;
    }
    out = static_cast<long>(parsed);
    return true;
}

} // namespace

void Insight::reloadConfigFromDisk() {
    auto& logger = getInstance().getSelf().getLogger();
    logger.info("Reloading configuration...");
    if (loadConfigFile(gConfig)) {
        logger.info("Configuration reloaded.");
    } else {
        // file was missing or out of date: persist the upgraded copy
        logger.info("Configuration upgraded to version {}", gConfig.version);
        saveConfigFile();
    }
    // platform loops read cfg() every round, so listeners pick up the new
    // values automatically.
}

ConfigEditResult
Insight::applyConfigEdit(std::string const& option, std::string const& value, std::string const& localeCode) {
    auto lower = util::toLower(option);

    // success paths persist immediately; failures only carry a message
    auto finish = [](ConfigEditResult result) {
        if (result.ok) {
            saveConfigFile();
        }
        return result;
    };

    auto setBool = [&](bool& target, char const* name) -> ConfigEditResult {
        bool b = false;
        if (!parseBool(value, b)) {
            return {false, tr(localeCode, "Invalid boolean value: {0} (use true/false, on/off, 1/0)", value)};
        }
        target = b;
        return finish({true, tr(localeCode, "set {0} = {1}", name, b ? "true" : "false")});
    };
    auto setNumber = [&](float& target, char const* name, double lo, double hi) -> ConfigEditResult {
        double d = 0;
        if (!parseDouble(value, d)) {
            return {false, tr(localeCode, "Invalid number value: {0}", value)};
        }
        if (d < lo || d > hi) {
            return {
                false,
                tr(localeCode,
                   "Value out of range for {0} ({1}..{2})",
                   name,
                   util::trimNumber(lo),
                   util::trimNumber(hi))
            };
        }
        target = static_cast<float>(d);
        return finish({true, tr(localeCode, "set {0} = {1}", name, util::trimNumber(d, 2))});
    };
    auto setText = [&](std::string& target, char const* name) -> ConfigEditResult {
        target = value;
        return finish({true, tr(localeCode, "set {0} = {1}", name, value)});
    };
    auto setEnum = [&](std::string&                            target,
                       char const*                             name,
                       std::initializer_list<std::string_view> allowed) -> ConfigEditResult {
        std::string list;
        for (auto candidate : allowed) {
            if (value == candidate) {
                target = value;
                return finish({true, tr(localeCode, "set {0} = {1}", name, value)});
            }
            list += (list.empty() ? "" : "|");
            list += candidate;
        }
        return {false, tr(localeCode, "Invalid value for {0} ({1})", name, list)};
    };
    auto setKeyCode = [&](int& target, char const* name) -> ConfigEditResult {
        long v = 0;
        if (!parseInt(value, v) || v < 0 || v > 255) {
            return {false, tr(localeCode, "Invalid key code: {0} (0..255, 0 = disabled)", value)};
        }
        target = static_cast<int>(v);
        return finish({true, tr(localeCode, "set {0} = {1}", name, std::to_string(v))});
    };
    // A colour is stored as the bare formatting code ("c"), but accepted in the
    // shapes a player is likely to type: "c", "§c", "&c", "#c", or "none" to go
    // back to the client's plain text colour.
    auto setColor = [&](std::string& target, char const* name) -> ConfigEditResult {
        auto code = normalizeColorCode(value);
        if (!code) {
            return {false, tr(localeCode, "Invalid colour code: {0} (0-9, a-f, or none)", value)};
        }
        target = *code;
        return finish({true, tr(localeCode, "set {0} = {1}", name, target.empty() ? "none" : target)});
    };

    // --- top-level options -----------------------------------------------
    if (lower == "enabled") {
        return setBool(gConfig.enabled, "enabled");
    }
    if (lower == "enabledbydefault") {
        return setBool(gConfig.enabledByDefault, "enabledByDefault");
    }
    if (lower == "maxdistance") {
        return setNumber(gConfig.maxDistance, "maxDistance", 1, 256);
    }
    if (lower == "intervalticks") {
        long v = 0;
        if (!parseInt(value, v) || v < 1 || v > 200) {
            return {false, tr(localeCode, "Invalid intervalTicks (1..200)")};
        }
        gConfig.intervalTicks = static_cast<int>(v);
        return finish({true, tr(localeCode, "set {0} = {1}", "intervalTicks", std::to_string(v))});
    }
    if (lower == "passthroughliquids") {
        return setBool(gConfig.passThroughLiquids, "passThroughLiquids");
    }
    if (lower == "showempty") {
        return setBool(gConfig.showEmpty, "showEmpty");
    }
    if (lower == "emptytext") {
        return setText(gConfig.emptyText, "emptyText");
    }
    if (lower == "entityenabled") {
        return setBool(gConfig.entityEnabled, "entityEnabled");
    }
    // The hit point / armor sprites: how many points still fit as sprites and how
    // many sprites fit on a row (EntityOptions::heartsThreshold / heartsPerRow), and
    // how the two rows are drawn at all (EntityOptions::healthStyle).
    if (lower == "entity.healthstyle") {
        return setEnum(
            gConfig.entity.healthStyle,
            "entity.healthStyle",
            {"bar", "bar+number", "hearts", "hearts+number", "number"}
        );
    }
    if (lower == "entity.heartsthreshold") {
        return setNumber(gConfig.entity.heartsThreshold, "entity.heartsThreshold", 2, 200);
    }
    if (lower == "entity.heartsperrow") {
        long v = 0;
        if (!parseInt(value, v) || v < 1 || v > 40) {
            return {false, tr(localeCode, "Invalid entity.heartsPerRow (1..40)")};
        }
        gConfig.entity.heartsPerRow = static_cast<int>(v);
        return finish({true, tr(localeCode, "set {0} = {1}", "entity.heartsPerRow", std::to_string(v))});
    }

    // --- display switches: one per field the panel can show (DisplayOptions).
    // These are what replaced the format / entityFormat text templates. ---
    {
        struct DisplaySwitch {
            char const* name;
            bool*       target;
        };
        for (auto const& entry : {
                 DisplaySwitch{"display.name",           &gConfig.display.name          },
                 DisplaySwitch{"display.identifier",     &gConfig.display.identifier    },
                 DisplaySwitch{"display.position",       &gConfig.display.position      },
                 DisplaySwitch{"display.distance",       &gConfig.display.distance      },
                 DisplaySwitch{"display.translationKey", &gConfig.display.translationKey},
                 DisplaySwitch{"display.facing",         &gConfig.display.facing        },
                 DisplaySwitch{"display.light",          &gConfig.display.light         },
                 DisplaySwitch{"display.emission",       &gConfig.display.emission      },
                 DisplaySwitch{"display.extras",         &gConfig.display.extras        },
             }) {
            // the option arrives lower-cased; the table keeps the canonical
            // spelling so messages show it as documented
            if (lower == util::toLower(entry.name)) {
                return setBool(*entry.target, entry.name);
            }
        }
    }

    // --- entity switches: the entity panel has its own layout and its own set of
    // fields (EntityOptions), so these are separate from the block ones above ---
    {
        struct EntitySwitch {
            char const* name;
            bool*       target;
        };
        for (auto const& entry : {
                 EntitySwitch{"entity.name",           &gConfig.entity.name          },
                 EntitySwitch{"entity.facing",         &gConfig.entity.facing        },
                 EntitySwitch{"entity.identifier",     &gConfig.entity.identifier    },
                 EntitySwitch{"entity.translationKey", &gConfig.entity.translationKey},
                 EntitySwitch{"entity.position",       &gConfig.entity.position      },
                 EntitySwitch{"entity.distance",       &gConfig.entity.distance      },
                 EntitySwitch{"entity.health",         &gConfig.entity.health        },
                 EntitySwitch{"entity.extras",         &gConfig.entity.extras        },
                 EntitySwitch{"entity.healthDecimals", &gConfig.entity.healthDecimals},
                 EntitySwitch{"entity.armor",          &gConfig.entity.armor         },
             }) {
            if (lower == util::toLower(entry.name)) {
                return setBool(*entry.target, entry.name);
            }
        }
    }

    // --- colours: one entry per piece of the panel (ColorOptions). Both panels
    // and the extras lines read from here, so these names are what the colour
    // scheme is configured with. ---
    {
        struct ColorSwitch {
            char const*  name;
            std::string* target;
        };
        for (auto const& entry : {
                 ColorSwitch{"colors.name",           &gConfig.colors.name          },
                 ColorSwitch{"colors.facing",         &gConfig.colors.facing        },
                 ColorSwitch{"colors.identifier",     &gConfig.colors.identifier    },
                 ColorSwitch{"colors.translationKey", &gConfig.colors.translationKey},
                 ColorSwitch{"colors.x",              &gConfig.colors.x             },
                 ColorSwitch{"colors.y",              &gConfig.colors.y             },
                 ColorSwitch{"colors.z",              &gConfig.colors.z             },
                 ColorSwitch{"colors.distance",       &gConfig.colors.distance      },
                 ColorSwitch{"colors.label",          &gConfig.colors.label         },
                 ColorSwitch{"colors.value",          &gConfig.colors.value         },
                 ColorSwitch{"colors.health",         &gConfig.colors.health        },
             }) {
            if (lower == util::toLower(entry.name)) {
                return setColor(*entry.target, entry.name);
            }
        }
    }

    // --- server side ------------------------------------------------------
    if (lower == "channel") {
        return setEnum(
            gConfig.server.channel,
            "channel",
            {"none", "actionbar", "tip", "jukebox", "system", "chat"}
        );
    }

    // --- client overlay ---------------------------------------------------
    if (lower == "showoverlay") {
        return setBool(gConfig.client.showOverlay, "showOverlay");
    }
    if (lower == "anchor") {
        return setEnum(
            gConfig.client.anchor,
            "anchor",
            {"top_left",
             "top_center",
             "top_right",
             "middle_left",
             "center",
             "middle_right",
             "bottom_left",
             "bottom_center",
             "bottom_right"}
        );
    }
    if (lower == "offsetx") {
        return setNumber(gConfig.client.offsetX, "offsetX", -1, 1);
    }
    if (lower == "offsety") {
        return setNumber(gConfig.client.offsetY, "offsetY", -1, 1);
    }
    if (lower == "fontsize") {
        return setNumber(gConfig.client.fontSize, "fontSize", 0.25, 4);
    }
    if (lower == "maxwidth") {
        return setNumber(gConfig.client.maxWidth, "maxWidth", 0, 1);
    }
    if (lower == "transitiontime") {
        return setNumber(gConfig.client.transitionTime, "transitionTime", 0, 1);
    }
    if (lower == "background") {
        return setBool(gConfig.client.background, "background");
    }
    if (lower == "backgroundalpha") {
        return setNumber(gConfig.client.backgroundAlpha, "backgroundAlpha", 0, 1);
    }
    if (lower == "shadow") {
        return setBool(gConfig.client.shadow, "shadow");
    }
    if (lower == "textcolor") {
        return setText(gConfig.client.textColor, "textColor");
    }
    if (lower == "language") {
        return setText(gConfig.client.language, "language");
    }
    if (lower == "hideoverlayingui") {
        return setBool(gConfig.client.hideOverlayInGui, "hideOverlayInGui");
    }
    if (lower == "overlayonremote") {
        return setEnum(gConfig.client.overlayOnRemote, "overlayOnRemote", {"on", "off"});
    }
    if (lower == "keyopenconfig") {
        return setKeyCode(gConfig.client.keyOpenConfig, "keyOpenConfig");
    }
    if (lower == "keytoggleshow") {
        return setKeyCode(gConfig.client.keyToggleShow, "keyToggleShow");
    }

    // --- extras adapters: one switch per adapter (see BlockExtrasConfig) ---
    {
        struct ExtraSwitch {
            char const* name;
            bool*       target;
        };
        for (auto const& entry : {
                 ExtraSwitch{"extras.enabled",         &gConfig.extras.enabled        },
                 ExtraSwitch{"extras.hardness",        &gConfig.extras.hardness       },
                 ExtraSwitch{"extras.blastResistance", &gConfig.extras.blastResistance},
                 ExtraSwitch{"extras.chest",           &gConfig.extras.chest          },
                 ExtraSwitch{"extras.bookshelf",       &gConfig.extras.bookshelf      },
                 ExtraSwitch{"extras.shelf",           &gConfig.extras.shelf          },
                 ExtraSwitch{"extras.lectern",         &gConfig.extras.lectern        },
                 ExtraSwitch{"extras.pot",             &gConfig.extras.pot            },
                 ExtraSwitch{"extras.brewing",         &gConfig.extras.brewing        },
                 ExtraSwitch{"extras.furnace",         &gConfig.extras.furnace        },
                 ExtraSwitch{"extras.jukebox",         &gConfig.extras.jukebox        },
                 ExtraSwitch{"extras.sign",            &gConfig.extras.sign           },
                 ExtraSwitch{"extras.banner",          &gConfig.extras.banner         },
                 ExtraSwitch{"extras.itemFrame",       &gConfig.extras.itemFrame      },
                 ExtraSwitch{"extras.flowerPot",       &gConfig.extras.flowerPot      },
                 ExtraSwitch{"extras.painting",        &gConfig.extras.painting       },
                 ExtraSwitch{"extras.piston",          &gConfig.extras.piston         },
                 ExtraSwitch{"extras.redstone",        &gConfig.extras.redstone       },
                 ExtraSwitch{"extras.repeater",        &gConfig.extras.repeater       },
                 ExtraSwitch{"extras.comparator",      &gConfig.extras.comparator     },
                 ExtraSwitch{"extras.dispenser",       &gConfig.extras.dispenser      },
                 ExtraSwitch{"extras.candle",          &gConfig.extras.candle         },
                 ExtraSwitch{"extras.respawnAnchor",   &gConfig.extras.respawnAnchor  },
                 ExtraSwitch{"extras.misc",            &gConfig.extras.misc           },
        }) {
            // the option arrives lower-cased; the table keeps the canonical
            // spelling so logs and messages show it as documented
            if (lower == util::toLower(entry.name)) {
                return setBool(*entry.target, entry.name);
            }
        }
    }

    return {false, tr(localeCode, "Unknown option: {0}", option)};
}

Insight& Insight::getInstance() {
    static Insight instance;
    return instance;
}

bool Insight::load() {
    auto& logger = mSelf.getLogger();
    logger.debug("Loading...");

    // Message catalogues: lang/<locale>.json next to the mod, loaded by the
    // LeviLamina i18n instance (see the i18n guide). A missing directory or a
    // broken file is not fatal - trRaw() then falls back to the key itself.
    if (auto loaded = ll::i18n::getInstance().load(mSelf.getLangDir()); !loaded) {
        logger.warn("Cannot load language files from {}", mSelf.getLangDir().string());
        loaded.error().log(logger);
    }

    // Load the configuration file; if it is missing or carries an older schema
    // version, loadConfigFile merges the current defaults in and we persist the
    // upgraded copy right away. A broken file must never keep the mod from
    // loading, so failures fall back to the defaults and rewrite the file.
    //
    // The old text templates are read from the raw file first: once ll::config has
    // merged and rewritten it, they are gone (see migrateLegacyDisplay). A file that
    // already carries the current version but an older shape is repaired here too.
    if (fillMissingGroups(configPath())) {
        logger.info("Configuration file was missing newer option groups; they were added with their defaults.");
    }
    std::string const configText  = readTextFile(configPath());
    long const        fileVersion = rawJsonInt(configText, "version", 0);
    bool const        migrate     = fileVersion < gConfig.version;

    bool loaded = false;
    try {
        loaded = loadConfigFile(gConfig);
    } catch (std::exception const& e) {
        logger.error("Cannot read {} ({}); falling back to the default configuration", configPath().string(), e.what());
        gConfig = Config{};
        saveConfigFile();
        loaded = true;
    }
    if (loaded) {
        logger.debug("Configuration loaded (version {}).", gConfig.version);
    } else {
        logger.info(
            "Configuration file was missing or outdated - saving upgraded configuration (version {}).",
            gConfig.version
        );
        saveConfigFile();
    }

    // A version <= 4 file described its panel with text templates, which no longer
    // exist: turn what they showed into display switches, so the panel looks the way
    // it did before the upgrade.
    if (migrate && migrateLegacyDisplay(configText, gConfig)) {
        logger.info("Migrated the old format / entityFormat templates into display switches.");
        saveConfigFile();
    }

#ifdef INSIGHT_TARGET_SERVER
    // Durable per-player data. KeyValueDB creates the directory when missing,
    // and being constructed here means every later access sees a ready database.
    playerDb = std::make_unique<ll::data::KeyValueDB>(mSelf.getDataDir() / "players");
    logger.debug("Player database ready at {}", (mSelf.getDataDir() / "players").string());
#endif
    return true;
}

bool Insight::enable() {
    auto& logger = mSelf.getLogger();
    logger.debug("Enabling...");

#ifdef INSIGHT_TARGET_SERVER
    mLogic = std::make_unique<ServerLogic>();
#else
    mLogic = std::make_unique<ClientLogic>();
#endif
    if (!mLogic->enable()) {
        logger.error("Platform logic failed to enable");
        mLogic.reset();
        return false;
    }
    return true;
}

bool Insight::disable() {
    auto& logger = mSelf.getLogger();
    logger.debug("Disabling...");
    if (mLogic) {
        mLogic->disable();
        mLogic.reset();
    }
    return true;
}

bool Insight::unload() { return disable(); }

} // namespace insight

LL_REGISTER_MOD(insight::Insight, insight::Insight::getInstance());
