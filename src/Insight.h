#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include "ll/api/mod/NativeMod.h"

#include "Config.h"
#include "PlatformLogic.h"

namespace insight {

// Result of one `/insight set <option> <value>` edit: `ok` tells the command
// whether to report success or failure, `message` is already localized.
struct ConfigEditResult {
    bool        ok = false;
    std::string message;
};

class Insight {

public:
    static Insight& getInstance();

    Insight() : mSelf(*ll::mod::NativeMod::current()) {}

    // NOTE: the destructor intentionally does nothing. At process exit the
    // function-local `instance` is destroyed during DLL detach, when the
    // loader has already torn the NativeMod down; touching the mod API (e.g.
    // its logger) there would dereference freed memory and crash. Disabling
    // is handled by the loader lifecycle (Insight::disable()).
    ~Insight() = default;

    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }

    /// @return True if the mod is loaded successfully.
    bool load();

    /// @return True if the mod is enabled successfully.
    bool enable();

    /// @return True if the mod is disabled successfully.
    bool disable();

    /// @return True if the mod is unloaded successfully.
    bool unload();

    // ---- global configuration access (main thread only) ----------------
    [[nodiscard]] static Config const& cfg();

    /// Re-read config.json from disk and replace the in-memory copy.
    static void reloadConfigFromDisk();

    /// Apply one config option from a command like `/insight set <option> <value>`.
    /// The message is translated for `localeCode` (empty = the default locale).
    [[nodiscard]] static ConfigEditResult
    applyConfigEdit(std::string const& option, std::string const& value, std::string const& localeCode);

    /// Path of the config file.
    [[nodiscard]] static std::filesystem::path configPath();

    // ---- durable per-player data (server only) -------------------------
    // Backed by a LeviLamina key-value database in the mod's data directory
    // (`<data dir>/players`), created in load(), so per-player settings survive
    // server restarts. On the client both functions are no-ops.

    /// The stored override of the master switch, or nullopt when this player
    /// has no entry (never toggled, or toggled back to the default).
    [[nodiscard]] static std::optional<bool> playerOverride(std::string const& uuid);

    /// Store (`true`/`false`) or delete (nullopt) the override for `uuid`.
    static void setPlayerOverride(std::string const& uuid, std::optional<bool> enabled);

private:
    ll::mod::NativeMod&            mSelf;
    std::unique_ptr<PlatformLogic> mLogic;
};

} // namespace insight
