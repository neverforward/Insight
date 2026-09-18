#include "Insight.h"

#include <cstdint>
#include <string>

#include "ll/api/Config.h"
#include "ll/api/i18n/I18n.h"
#include "ll/api/mod/RegisterHelper.h"
#ifdef INSIGHT_TARGET_SERVER
#include "ll/api/data/KeyValueDB.h"
#endif

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

bool parseBool(std::string const& value, bool& out) {
    auto v = util::toLower(value);
    if (v == "true" || v == "1" || v == "yes" || v == "on") {
        out = true;
        return true;
    }
    if (v == "false" || v == "0" || v == "no" || v == "off") {
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
    if (lower == "format") {
        return setText(gConfig.format, "format");
    }
    if (lower == "entityenabled") {
        return setBool(gConfig.entityEnabled, "entityEnabled");
    }
    if (lower == "entityformat") {
        return setText(gConfig.entityFormat, "entityFormat");
    }

    // --- server side ------------------------------------------------------
    if (lower == "channel") {
        return setEnum(
            gConfig.server.channel,
            "channel",
            {"none", "actionbar", "tip", "popup", "jukebox", "system", "chat"}
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

    // --- extras adapters --------------------------------------------------
    if (lower == "extras.enabled") {
        return setBool(gConfig.extras.enabled, "extras.enabled");
    }
    if (lower == "extras.chest") {
        return setBool(gConfig.extras.chest, "extras.chest");
    }
    if (lower == "extras.furnace") {
        return setBool(gConfig.extras.furnace, "extras.furnace");
    }
    if (lower == "extras.brewing") {
        return setBool(gConfig.extras.brewing, "extras.brewing");
    }
    if (lower == "extras.redstone") {
        return setBool(gConfig.extras.redstone, "extras.redstone");
    }
    if (lower == "extras.misc") {
        return setBool(gConfig.extras.misc, "extras.misc");
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
