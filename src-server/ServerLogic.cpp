#include "ServerLogic.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>

#include "nlohmann/json.hpp"

#include "ll/api/Config.h"
#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/CommandRegistrar.h"
#include "ll/api/event/command/ServerCommandRegisterEvent.h"
#include "ll/api/event/player/PlayerDisconnectEvent.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"
#include "mc/deps/core/math/Vec2.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/network/packet/SetTitlePacket.h"
#include "mc/network/packet/TextPacket.h"
#include "mc/network/packet/TextPacketType.h"
#include "mc/server/ServerPlayer.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/level/dimension/DimensionType.h"

#include "Config.h"
#include "Extras.h"
#include "EntityTarget.h"
#include "Format.h"
#include "Insight.h"
#include "InsightCommand.h"
#include "Raycast.h"

namespace insight {

namespace {

// ---------------------------------------------------------------------------
// packet helpers
// ---------------------------------------------------------------------------
void sendTextPacket(ServerPlayer& player, TextPacketType type, std::string const& text) {
    TextPacket pkt;
    TextPacketPayload::MessageOnly message;
    message.mType = type;
    message.mMessage->assign(text);
    pkt.mBody = message;
    player.sendNetworkPacket(pkt);
}

void sendActionbar(ServerPlayer& player, std::string const& text) {
    SetTitlePacket pkt(SetTitlePacketPayload::TitleType::Actionbar, text, std::optional<std::string>{text});
    pkt.mFadeInTime        = 0;
    pkt.mStayTime          = 0;
    pkt.mFadeOutTime       = 0;
    pkt.mXuid              = player.getXuid();
    pkt.mPlatformOnlineId  = *player.mPlatformOnlineId; 
    player.sendNetworkPacket(pkt);
}

// Push `text` to one player using the configured channel.
void sendViaChannel(ServerPlayer& player, std::string const& channel, std::string const& text) {
    if (channel == "none") {
        return;
    }
    if (channel == "actionbar") {
        sendActionbar(player, text);
    } else if (channel == "tip") {
        sendTextPacket(player, TextPacketType::Tip, text);
    } else if (channel == "popup") {
        sendTextPacket(player, TextPacketType::Popup, text);
    } else if (channel == "jukebox") {
        auto pkt = TextPacketPayload::createJukeboxPopup(text, {});
        player.sendNetworkPacket(pkt);
    } else if (channel == "system") {
        auto pkt = TextPacketPayload::createSystemMessage(text);
        player.sendNetworkPacket(pkt);
    } else if (channel == "chat") {
        auto pkt = TextPacket::createRawMessage(text);
        player.sendNetworkPacket(pkt);
        } else { // unknown channel -> actionbar
        sendActionbar(player, text);
    }
}

// The dimension carries its own name ("overworld" / "nether" / "the_end"),
// so no id -> name table is needed.
std::string dimensionName(BlockSource const& region) {
    try {
        return region.getDimension().mName;
    } catch (...) {
        return std::to_string(static_cast<int>(region.getDimensionId()));
    }
}

// ---------------------------------------------------------------------------
// per-player overrides: uuid -> enabled
//
// The key-value database owned by Insight is the source of truth (so a
// player's switch survives server restarts); gOverrides is only a cache that
// avoids a database lookup on every sampling round, and gLoaded remembers which
// players were already read so a missing entry is not looked up again.
// ---------------------------------------------------------------------------
std::unordered_map<std::string, bool> gOverrides;
std::unordered_set<std::string>       gLoaded;

} // namespace
namespace settings {

std::optional<bool> getEnabled(std::string const& uuid) {
    if (auto it = gOverrides.find(uuid); it != gOverrides.end()) {
        return it->second;
    }
    if (gLoaded.insert(uuid).second) { // first time we see this player
        if (auto stored = Insight::playerOverride(uuid)) {
            gOverrides.emplace(uuid, *stored);
            return stored;
        }
    }
    return std::nullopt;
}

void setEnabled(std::string const& uuid, bool on) {
    gLoaded.insert(uuid);
    if (on == Insight::cfg().enabledByDefault) {
        // the default value needs no stored entry
        gOverrides.erase(uuid);
        Insight::setPlayerOverride(uuid, std::nullopt);
        return;
    }
    gOverrides[uuid] = on;
    Insight::setPlayerOverride(uuid, on);
}

/// Forget the in-memory cache; the database entries stay (that is the point).
void resetCache() {
    gOverrides.clear();
    gLoaded.clear();
}

/// Drop one player from the cache (they keep their stored value).
void forget(std::string const& uuid) {
    gOverrides.erase(uuid);
    gLoaded.erase(uuid);
}

} // namespace settings

// ---------------------------------------------------------------------------
// ServerLogic
// ---------------------------------------------------------------------------
// Destructor intentionally empty: teardown happens in disable(), which the
// mod lifecycle calls before the loader releases the module. During DLL
// detach / process exit nothing here must touch the loader again.
ServerLogic::~ServerLogic() = default;

bool ServerLogic::enable() {
    auto& logger = Insight::getInstance().getSelf().getLogger();

    settings::resetCache();

    auto& bus = ll::event::EventBus::getInstance();

    // cadence loop: sample all online players' look rays on a fixed cadence
    mListeners.emplace_back(bus.emplaceListener<ll::event::ServerLevelTickEvent>(
        [this](ll::event::ServerLevelTickEvent& event) { onTick(event); }
    ));

    // disconnected players only leave the cache; the database keeps their entry
    mListeners.emplace_back(bus.emplaceListener<ll::event::player::PlayerDisconnectEvent>(
        [](ll::event::player::PlayerDisconnectEvent& event) {
            settings::forget(event.self().getUuid().asString());
        }
    ));

    // /insight command (registered when the engine is ready for it); shared
    // command tree, plus the per-player on/off/toggle switches
    mListeners.emplace_back(bus.emplaceListener<ll::event::command::ServerCommandRegisterEvent>([](auto&) {
        registerInsightCommand(
            false,
            [](Player& player) -> bool {
                auto& sp   = *static_cast<ServerPlayer*>(&player);
                bool   cur = settings::getEnabled(sp.getUuid().asString()).value_or(Insight::cfg().enabledByDefault);
                settings::setEnabled(sp.getUuid().asString(), !cur);
                Insight::getInstance().getSelf().getLogger().info("{} toggled Insight {}", sp.getRealName(),
                                                                  cur ? "off" : "on");
                return !cur;
            }
        );
    }));

    logger.info("Server mode enabled. channel={} interval={}t maxDistance={}", Insight::cfg().server.channel,
                Insight::cfg().intervalTicks, Insight::cfg().maxDistance);
    return true;
}

void ServerLogic::disable() {
    auto& bus = ll::event::EventBus::getInstance();
    for (auto& l : mListeners) {
        bus.removeListener(l);
    }
    mListeners.clear();
    settings::resetCache();
}

void ServerLogic::onTick(ll::event::ServerLevelTickEvent& event) {
    auto const& cfg = Insight::cfg();
    if (!cfg.enabled) {
        return;
    }
    int interval = cfg.intervalTicks > 0 ? cfg.intervalTicks : 1;
    if ((++mTickCounter) % interval != 0) {
        return;
    }

    auto maxDist = static_cast<double>(cfg.maxDistance);

    // last "[look]" diagnostic per player: emitted only when it changes
    static std::map<std::string, std::string> lastLogged;

    event.level().forEachPlayer([&](Player& player) {
        if (player.isSimulated()) {
            return true;
        }
        auto& sp = *static_cast<ServerPlayer*>(&player);

        bool on = settings::getEnabled(sp.getUuid().asString()).value_or(cfg.enabledByDefault);
        if (!on) {
            return true;
        }

        Vec3  origin = player.getEyePos();
        Vec3  dir    = player.getViewVector(0.0f);
        auto& region = player.getDimensionBlockSource();
        auto  hit    = raycastToBlock(region, origin, dir, maxDist, cfg.passThroughLiquids);
        double blockDist = hit ? hit->distance : 1e30;

        // nearest entity on the same look ray (takes priority over the block
        // only when it is closer)
        Actor* entity     = nullptr;
        double entityDist = 0;
        if (cfg.entityEnabled) {
            auto entHit = findLookEntity(region, &player, origin, dir, maxDist);
            entity      = entHit.actor;
            entityDist  = entHit.distance;
            if (entity && entityDist > blockDist) {
                entity = nullptr; // the block in front wins
            }
        }

        std::string text;
        if (entity) {
            std::string eType = entity->getTypeName();
            bool        hasHp = entityHasHealth(*entity);
            auto        info  = makeEntityLookInfo(
                entityDisplayName(entity, sp.getLanguageCode()),
                eType,
                hasHp ? entity->getHealth() : 0,
                hasHp ? entity->getMaxHealth() : 0
            );
            info.distance = entityDist;
            info.dimName  = dimensionName(region);
            info.extras   = buildEntityExtras(region, *entity, sp.getLanguageCode(), cfg.extras);
            text          = renderText(cfg, info);
        } else if (hit) {
            auto info = makeBlockLookInfo(*hit, sp.getLanguageCode());
            info.dimName   = dimensionName(region);
            info.extras    = buildBlockExtras(region, hit->pos, hit->typeName, sp.getLanguageCode(), cfg.extras);
            info.direction = describeBlockFacing(region, hit->pos, sp.getLanguageCode());
            info.light     = describeBlockLight(region, hit->pos);
            info.emission  = describeBlockEmission(region, hit->pos);
            text           = renderText(cfg, info);
        } else if (cfg.showEmpty) {
            LookInfo info;
            text = renderText(cfg, info);
        } else {
            return true;
        }

        // diagnostics: one line per *change* of the sampled target, so a player
        // staring at a block does not log on every sampling round
        std::string const target = entity ? entity->getTypeName()
                                 : hit  ? hit->typeName + " " + hit->descriptionId
                                        : std::string("<none>");
        std::string const line   = target + "|" + text;
        if (auto& last = lastLogged[sp.getUuid().asString()]; last != line) {
            last = line;
            Insight::getInstance().getSelf().getLogger().debug(
                "[{}] target={} text={}",
                sp.getRealName(),
                target,
                text
            );
        }
        sendViaChannel(sp, cfg.server.channel, text);
        return true;
    });
}

} // namespace insight
