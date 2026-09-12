#pragma once

#include <optional>
#include <string>
#include <vector>

#include "ll/api/event/EventBus.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"

#include "PlatformLogic.h"

namespace insight {

// Server (BDS / LeviLamina server) implementation: on a fixed cadence samples
// every online player's look ray and pushes the formatted text to *that*
// player only, through the configured display channel.
class ServerLogic final : public PlatformLogic {
public:
    ServerLogic()  = default;
    ~ServerLogic() override;

    bool enable() override;
    void disable() override;

private:
    void onTick(ll::event::ServerLevelTickEvent& event);

    std::vector<ll::event::ListenerPtr> mListeners;
    uint64                              mTickCounter = 0;
};

// Per-player switch handling, shared with the command handlers. The values live
// in a key-value database (see Insight::playerOverride); these helpers add the
// in-memory cache and the "equal to the default = no entry" rule.
namespace settings {
// enabled override for a player (nullopt = follow the config default)
std::optional<bool> getEnabled(std::string const& uuid);
void                setEnabled(std::string const& uuid, bool on);
void                resetCache();
void                forget(std::string const& uuid);
} // namespace settings

} // namespace insight
