#include "Extras.h"

#include <array>
#include <bitset>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "mc/deps/core/string/HashedString.h"
#include "mc/deps/shared_types/legacy/actor/ArmorSlot.h"
#include "mc/entity/components_json_legacy/ContainerComponent.h"
#include "mc/world/Container.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/Motif.h"
#include "mc/world/actor/Painting.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemStackBase.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockType.h"
#include "mc/world/level/block/VanillaStates.h"
#include "mc/world/level/block/actor/BannerBlockActor.h"
#include "mc/world/level/block/actor/BeaconBlockActor.h"
#include "mc/world/level/block/actor/BeehiveBlockActor.h"
#include "mc/world/level/block/actor/BlockActor.h"
#include "mc/world/level/block/actor/BlockActorType.h"
#include "mc/world/level/block/actor/BrewingStandBlockActor.h"
#include "mc/world/level/block/actor/CampfireBlockActor.h"
#include "mc/world/level/block/actor/ComparatorBlockActor.h"
#include "mc/world/level/block/actor/CrafterBlockActor.h"
#include "mc/world/level/block/actor/DecoratedPotBlockActor.h"
#include "mc/world/level/block/actor/FlowerPotBlockActor.h"
#include "mc/world/level/block/actor/FurnaceBlockActor.h"
#include "mc/world/level/block/actor/ItemFrameBlockActor.h"
#include "mc/world/level/block/actor/JukeboxBlockActor.h"
#include "mc/world/level/block/actor/LecternBlockActor.h"
#include "mc/world/level/block/actor/PistonBlockActor.h"
#include "mc/world/level/block/actor/PistonState.h"
#include "mc/world/level/block/actor/ShelfBlockActor.h"
#include "mc/world/level/block/actor/SignBlockActor.h"
#include "mc/world/level/block/actor/component/IVanillaMainBlockActorComponent.h"
#include "mc/world/level/block/components/BlockFlammableComponent.h"
#include "mc/world/level/block/components/BlockInstrumentComponent.h"
#include "mc/world/level/block/states/BlockStateInstance.h"

#include "ll/api/io/Logger.h"

#include "I18n.h"
#include "Insight.h"
#include "Translation.h"
#include "Util.h"

namespace insight {

namespace {

bool contains(std::string const& s, char const* part) { return s.find(part) != std::string::npos; }

// ---------------------------------------------------------------------------
// localization: engine i18n, exact key first then "<key>.name" (vanilla lang
// file convention); on failure show the raw translation key itself.
// ---------------------------------------------------------------------------
std::string localizeKey(std::string const& langCode, std::string const& key) {
    if (key.empty()) {
        return {};
    }
    return resolveDisplayName(key, langCode);
}

// ---------------------------------------------------------------------------
// generic block-state reader: resolves a state property *by name* through the
// block type's state-name table, then reads this block's value.
// ---------------------------------------------------------------------------

bool regionLoaded(IConstBlockSource const& region, BlockPos const& pos);

// Read a state through the engine's own typed handle (VanillaStates::Candles()
// and friends). This works for data-driven blocks whose legacy name -> id table
// is empty, which is why the name-based lookup silently found nothing for them.
template <class T>
std::optional<T>
readVanillaState(IConstBlockSource const& region, BlockPos const& pos, BlockStateVariant<T> const& state) {
    if (!regionLoaded(region, pos)) {
        return std::nullopt;
    }
    try {
        return region.getBlock(pos).getState<T>(state);
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<int> readStateInt(IConstBlockSource const& region, BlockPos const& pos, char const* stateName) {
    try {
        auto const& block = region.getBlock(pos);
        auto const& type  = block.getBlockType();
        // linear lookup is fine here (property tables are small)
        uint64 id    = 0;
        bool   found = false;
        for (auto const& [name, stateId] : *type.mStateNameMap) {
            if (name.getString() == stateName) {
                id    = stateId;
                found = true;
                break;
            }
        }
        if (!found) {
            // Data-driven blocks (minecraft:noteblock, ...) register no name in
            // the legacy table: their states live in mStates, where each instance
            // knows its BlockState and therefore its name.
            for (auto const& [stateId, instance] : *type.mStates) {
                if (instance.mState->mName->getString() == stateName) {
                    return block.getState<int>(stateId);
                }
            }
            for (auto const& collection : *type.mAlteredStateCollections) {
                if (collection && collection->mBlockState->get().mName->getString() == stateName) {
                    return block.getState<int>(collection->mBlockState->get().mID);
                }
            }
            return std::nullopt;
        }
        return block.getState<int>(id);
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<int>
readStateIntAny(IConstBlockSource const& region, BlockPos const& pos, std::initializer_list<char const*> names) {
    for (auto const* n : names) {
        if (auto v = readStateInt(region, pos, n)) {
            return v;
        }
    }
    return std::nullopt;
}

std::optional<bool> readStateBool(IConstBlockSource const& region, BlockPos const& pos, char const* stateName) {
    if (auto v = readStateInt(region, pos, stateName)) {
        return *v != 0;
    }
    return std::nullopt;
}

std::string valueLine(std::string const& langCode, char const* labelKey, int value) {
    return std::string("§7") + tr(langCode, labelKey) + " §f" + std::to_string(value);
}

std::string textLine(std::string const& langCode, char const* labelKey, std::string const& value) {
    return std::string("§7") + tr(langCode, labelKey) + " §f" + value;
}

// ---------------------------------------------------------------------------
// Debug diagnostics: these run on every sampling round, so they are emitted
// once per change (same tag + position + message) instead of on every round.
// The log level decides whether they show up at all.
// ---------------------------------------------------------------------------
void logOnce(ll::io::Logger& logger, char const* tag, BlockPos const& pos, std::string const& message) {
    static std::map<std::tuple<std::string, int, int, int>, std::string> lastLogged;

    if (!logger.shouldLog(ll::io::LogLevel::Debug)) {
        return;
    }
    // the bookkeeping is only a throttle for the log: drop it when it grows
    if (lastLogged.size() > 512) {
        lastLogged.clear();
    }
    auto key = std::make_tuple(std::string(tag), pos.x, pos.y, pos.z);
    if (auto it = lastLogged.find(key); it != lastLogged.end() && it->second == message) {
        return;
    }
    lastLogged[key] = message;
    logger.debug("[{}] ({},{},{}) {}", tag, pos.x, pos.y, pos.z, message);
}

// ---------------------------------------------------------------------------
// direction helpers (Bedrock state value conventions)
// ---------------------------------------------------------------------------
std::string facingWord(std::string const& langCode, int code) {
    // 0 = down, 1 = up, 2 = north, 3 = south, 4 = west, 5 = east
    switch (code) {
    case 0:
        return tr(langCode, "down");
    case 1:
        return tr(langCode, "up");
    case 2:
        return tr(langCode, "north");
    case 3:
        return tr(langCode, "south");
    case 4:
        return tr(langCode, "west");
    case 5:
        return tr(langCode, "east");
    default:
        return {};
    }
}

std::string compassWord(std::string const& langCode, int code) {
    // 0 = south, 1 = west, 2 = north, 3 = east
    switch (((code % 4) + 4) % 4) {
    case 0:
        return tr(langCode, "south");
    case 1:
        return tr(langCode, "west");
    case 2:
        return tr(langCode, "north");
    default:
        return tr(langCode, "east");
    }
}

// BlockPos offset for a facing_direction value (0 down, 1 up, 2 north, 3 south,
// 4 west, 5 east), used for piston-arm / attachment checks.
BlockPos facingOffset(int code) {
    switch (code) {
    case 0:
        return BlockPos(0, -1, 0);
    case 1:
        return BlockPos(0, 1, 0);
    case 2:
        return BlockPos(0, 0, -1);
    case 3:
        return BlockPos(0, 0, 1);
    case 4:
        return BlockPos(-1, 0, 0);
    case 5:
        return BlockPos(1, 0, 0);
    default:
        return BlockPos(0, 0, 0);
    }
}

// Reading a block from an unloaded chunk is undefined; guard every read of a
// neighbour / scanned position with the same chunk test the raycast uses.
bool regionLoaded(IConstBlockSource const& region, BlockPos const& pos) {
    try {
        return region.hasChunksAt(pos, 0, false);
    } catch (...) {
        return false;
    }
}

std::string blockTypeAt(IConstBlockSource const& region, BlockPos const& pos) {
    if (!regionLoaded(region, pos)) {
        return {};
    }
    try {
        return region.getBlock(pos).getTypeName();
    } catch (...) {
        return {};
    }
}

bool isAirAt(IConstBlockSource const& region, BlockPos const& pos) {
    if (!regionLoaded(region, pos)) {
        return false;
    }
    try {
        return region.getBlock(pos).isAir();
    } catch (...) {
        return false;
    }
}

// ---------------------------------------------------------------------------
// live container snapshot (client): see setLiveContainerSnapshot()
// ---------------------------------------------------------------------------
struct ContainerSnapshot {
    int filled     = 0;
    int total      = 0;
    int totalCount = 0;
};

std::map<std::tuple<int, int, int>, ContainerSnapshot>& liveSnapshots() {
    static std::map<std::tuple<int, int, int>, ContainerSnapshot> map;
    return map;
}

std::map<int64_t, ContainerSnapshot>& liveEntitySnapshots() {
    static std::map<int64_t, ContainerSnapshot> map;
    return map;
}

// ---------------------------------------------------------------------------
// containers: slot scanning (data available server-side and in local worlds)
// ---------------------------------------------------------------------------
// 26.40 moved container access off BlockActor: an actor now hands out its "main
// component", and that component owns the container. Every container actor
// implements it (chest, barrel, hopper, furnace, brewing stand, ...), so this
// stays engine-driven instead of listing block actor types.
Container const* containerOf(BlockActor const* actor) {
    if (!actor) {
        return nullptr;
    }
    auto const* main = actor->_getMainComponent();
    return main ? main->getContainer() : nullptr;
}


// Compact "name ×count" list of the slots that are not empty (shelf, chiseled
// bookshelf). The interesting part of those blocks is what they hold, while
// chestLine() already reports how full they are.
std::string itemListLine(Container const& container, std::string const& langCode) {
    std::string out;
    int         shown = 0;
    for (int i = 0; i < container.getContainerSize(); ++i) {
        auto const& stack = container.getItem(i);
        if (stack.isNull()) {
            continue;
        }
        if (shown == 4) {
            out += " §8...";
            break;
        }
        if (!out.empty()) {
            out += "§7, ";
        }
        out += "§f" + localizeKey(langCode, stack.getDescriptionId());
        if (stack.mCount > 1) {
            out += "§7x" + std::to_string(stack.mCount);
        }
        ++shown;
    }
    return out;
}

std::string chestLine(BlockActor const* be, BlockPos const& pos, std::string const& langCode) {
    if (!be) {
        return {};
    }
    auto* c = containerOf(be);
    if (!c) {
        return {};
    }
    int size = c->getContainerSize();
    if (size <= 0) {
        return {};
    }
    int filled = 0;
    int total  = 0;
    for (int i = 0; i < size; ++i) {
        auto const& st = c->getItem(i);
        if (!st.isNull()) {
            ++filled;
            total += st.mCount;
        }
    }

    // Prefer the live snapshot taken from an open container UI: the engine's
    // world copy of a container keeps its old contents until the chunk is
    // reloaded, so the player's own edits would otherwise not show up.
    int   worldFilled  = filled;
    int   worldTotal   = total;
    bool  usedSnapshot = false;
    auto& snapshots    = liveSnapshots();
    auto  key          = std::make_tuple(pos.x, pos.y, pos.z);
    if (auto it = snapshots.find(key); it != snapshots.end()) {
        auto const& snap = it->second;
        if (snap.total == size && snap.filled == filled && snap.totalCount == total) {
            snapshots.erase(it); // the world copy caught up: drop the snapshot
        } else {
            filled       = snap.filled;
            total        = snap.totalCount;
            usedSnapshot = true;
        }
    }
    {
        logOnce(
            Insight::getInstance().getSelf().getLogger(),
            "chest",
            pos,
            "world=" + std::to_string(worldFilled) + "/" + std::to_string(size) + " (total "
                + std::to_string(worldTotal) + ") shown=" + std::to_string(filled) + "/" + std::to_string(size)
                + " (total " + std::to_string(total) + ") used_snapshot=" + (usedSnapshot ? "yes" : "no")
        );
    }

    std::string s = "§7" + tr(langCode, "Items") + " §f" + std::to_string(filled) + "/" + std::to_string(size);
    if (total > 0) {
        s += " §7· §f" + std::to_string(total);
    }
    return s;
}

std::string machineLine(BlockActor const* be, BlockPos const& pos, std::string const& langCode) {
    if (!be) {
        return {};
    }
    auto* c = containerOf(be);
    if (!c) {
        return {};
    }
    int size = c->getContainerSize();
    if (size <= 0) {
        return {};
    }
    int filled = 0;
    for (int i = 0; i < size; ++i) {
        if (!c->getItem(i).isNull()) {
            ++filled;
        }
    }
    // Furnaces / brewing stands are block containers too: prefer the live
    // snapshot captured from the open UI for the same reason as chestLine().
    auto& snapshots = liveSnapshots();
    auto  key       = std::make_tuple(pos.x, pos.y, pos.z);
    if (auto it = snapshots.find(key); it != snapshots.end()) {
        auto const& snap = it->second;
        if (snap.total == size && snap.filled == filled) {
            snapshots.erase(it);
        } else {
            filled = snap.filled;
        }
    }
    return "§7" + tr(langCode, "Slots") + " §f" + std::to_string(filled) + "/" + std::to_string(size);
}

// First non-empty slot of a container as a localized item name ("" if empty).
std::string firstItemName(BlockActor const* be, std::string const& langCode) {
    if (!be) {
        return {};
    }
    auto* c = containerOf(be);
    if (!c) {
        return {};
    }
    for (int i = 0; i < c->getContainerSize(); ++i) {
        auto const& st = c->getItem(i);
        if (!st.isNull()) {
            return localizeKey(langCode, st.getDescriptionId());
        }
    }
    return {};
}

// Container-ness is engine data (the block type knows whether it is a
// container), so no list of container block ids is needed here.
bool isContainerBlock(IConstBlockSource const& region, BlockPos const& pos) {
    if (!regionLoaded(region, pos)) {
        return false;
    }
    try {
        return region.getBlock(pos).getBlockType().isContainerBlock();
    } catch (...) {
        return false;
    }
}

// Modern versions encode the plant in the block itself, and the engine exposes
// no flag for "potted plant" - but the block does name itself accordingly
// ("tile.potted_<plant>"), which covers every current and future potted plant
// without a list of block ids.
bool isPottedBlock(IConstBlockSource const& region, BlockPos const& pos) {
    if (!regionLoaded(region, pos)) {
        return false;
    }
    try {
        return region.getBlock(pos).getDescriptionId().rfind("tile.potted_", 0) == 0;
    } catch (...) {
        return false;
    }
}

// ---------------------------------------------------------------------------
// comparator: the block state carries the value the engine just wrote, so it is
// read first; the block actor is only a fallback. Reading the actor first made
// the line report 0 forever: its signal is written on the server tick, while the
// state is already correct when the client looks at it.
// ---------------------------------------------------------------------------
std::string comparatorLine(
    IConstBlockSource const& region,
    BlockPos const&          pos,
    BlockActor const*        be,
    std::string const&       langCode
) {
    if (auto v = readStateIntAny(region, pos, {"output_signal", "powered"})) {
        return valueLine(langCode, "Signal", *v);
    }
    if (be && be->getType() == BlockActorType::Comparator) {
        int signal = const_cast<ComparatorBlockActor*>(static_cast<ComparatorBlockActor const*>(be))->getOutputSignal();
        return valueLine(langCode, "Signal", signal);
    }
    return {};
}

// ---------------------------------------------------------------------------
// misc states: machines and the block states that describe them.
//
// The engine's own state names drive everything here: a block that reports
// `open_bit` is a door/trapdoor/fence gate/lever, one that reports `crafting`
// is a crafter, one that reports `composter_fill_level` is a composter, and so
// on. That is why this function needs no list of block ids - it simply reports
// the states the block actually has. Only two cases cannot be expressed that
// way and are marked below (the piston arm block id, and the enchanting table
// whose power comes from its surroundings).
// ---------------------------------------------------------------------------

void miscStateLines(
    IConstBlockSource const&  region,
    BlockPos const&           pos,
    std::string const&        type,
    std::string const&        langCode,
    std::vector<std::string>& lines,
    BlockExtrasConfig const&  opt
) {
    BlockActor const* be = region.getBlockEntity(pos);

    // ---- binary states ---------------------------------------------------
    if (auto v = readStateBool(region, pos, "open_bit")) { // doors, trapdoors, gates, levers
        lines.push_back(textLine(langCode, "State", tr(langCode, *v ? "open" : "closed")));
    }
    if (auto v = readStateBool(region, pos, "powered")) { // redstone power (doors, ...)
        if (*v) {
            lines.push_back(textLine(langCode, "Powered", tr(langCode, "yes")));
        }
    }
    if (auto v = readStateBool(region, pos, "button_pressed_bit")) {
        lines.push_back(textLine(langCode, "State", tr(langCode, *v ? "pressed" : "released")));
    }
    if (auto v = readStateBool(region, pos, "connected_bit")) { // tripwire hooks
        lines.push_back(textLine(langCode, "Connected", tr(langCode, *v ? "yes" : "no")));
    }
    if (auto v = readStateBool(region, pos, "occupied_bit")) { // bed
        lines.push_back(textLine(langCode, "Occupied", tr(langCode, *v ? "yes" : "no")));
    }
    if (auto v = readStateBool(region, pos, "powered_bit")) { // observers, tripwire hooks, ...
        lines.push_back(textLine(langCode, "Powered", tr(langCode, *v ? "yes" : "no")));
    }
    // A crafter has exactly two states of its own: `crafting` (mouth open, top
    // glowing) and `triggered_bit` (it was activated). Bedrock spells booleans
    // with the `_bit` suffix - reading the Java spelling "triggered" matched no
    // state at all, which is why only one of the two ever showed up. Its disabled
    // slots are not a state either: they live in the block entity.
    auto const crafting = readStateBool(region, pos, "crafting");
    if (crafting) {
        lines.push_back(textLine(langCode, "State", tr(langCode, *crafting ? "crafting" : "idle")));
        if (auto v = readStateBool(region, pos, "triggered_bit")) {
            lines.push_back(textLine(langCode, "Triggered", tr(langCode, *v ? "yes" : "no")));
        }
        if (be && be->getType() == BlockActorType::Crafter) {
            auto const* crafter  = static_cast<CrafterBlockActor const*>(be);
            int const   disabled = static_cast<int>(crafter->mDisabledSlots->count());
            if (disabled > 0) {
                lines.push_back(valueLine(langCode, "Disabled slots", disabled));
            }
        }
    }

    // ---- numeric states --------------------------------------------------
    if (auto v = readStateInt(region, pos, "composter_fill_level")) {
        lines.push_back("§7" + tr(langCode, "Compost") + " §f" + std::to_string(*v) + "/8");
    }
    if (auto v = readStateInt(region, pos, "bite_counter")) { // cake
        lines.push_back("§7" + tr(langCode, "Slices") + " §f" + std::to_string(7 - *v) + "/7");
    }
    // TODO(26.40 + LeviLamina): the note block gives us nothing to read. Verified
    // in-game for minecraft:noteblock: BlockType::mStateNameMap is empty, mStates is
    // empty, mAlteredStateCollections has no named state and Block::getData() stays
    // 0, so neither the pitch nor the instrument can be obtained (VanillaStates has
    // no Note() handle either, and the instrument component lookup reads unrelated
    // memory, like BlockFlammableComponent did). The note pitch / instrument lines
    // and the extras.noteBlock switch were removed together; re-add them once the
    // data is reachable.
    if (auto v = readStateInt(region, pos, "cluster_count")) { // sea pickle
        lines.push_back(valueLine(langCode, "Count", *v + 1));
    }

    // ---- a few more states ----------------------------------
    if (auto v = readVanillaState(region, pos, VanillaStates::BeehiveHoneyLevel())) { // beehive / bee nest
        lines.push_back(textLine(langCode, "Honey level", std::to_string(*v) + "/5"));
    }
    if (auto v = readVanillaState(region, pos, VanillaStates::Extinguished())) { // campfire, soul campfire
        lines.push_back(textLine(langCode, "State", tr(langCode, *v ? "extinguished" : "lit")));
    }

    // ---- redstone components (one switch each) ---------------------------
    if (opt.repeater) {
        auto delay = readVanillaState(region, pos, VanillaStates::RepeaterDelay());
        if (!delay) {
            delay = readStateInt(region, pos, "repeater_delay");
        }
        if (delay) { // 0..3 -> 1..4
            lines.push_back(textLine(langCode, "Delay", std::to_string(*delay + 1) + "/4"));
        }
        // a repeater stores no signal level: it either passes full power or none
        auto powered = readVanillaState(region, pos, VanillaStates::PoweredBit());
        if (!powered) {
            powered = readStateBool(region, pos, "powered");
        }
        if (powered) {
            lines.push_back(textLine(langCode, "Signal", *powered ? "15" : "0"));
        }
    }
    if (opt.dispenser && !crafting) {
        // dispensers and droppers keep the state of their last activation; a
        // crafter has the same state name but reports it as its own (above)
        if (auto v = readStateBool(region, pos, "triggered_bit")) {
            lines.push_back(textLine(langCode, "State", tr(langCode, *v ? "triggered" : "idle")));
        }
    }
    if (opt.candle) {
        auto candles = readVanillaState(region, pos, VanillaStates::Candles());
        if (!candles) {
            candles = readStateInt(region, pos, "candles");
        }
        if (candles) {
            auto const v = candles;
            // the engine counts candles from 0: a single candle reports 0
            lines.push_back(textLine(langCode, "Candles", std::to_string(*v + 1) + "/4"));
        }
        auto lit = readVanillaState(region, pos, VanillaStates::Lit());
        if (!lit) {
            lit = readStateBool(region, pos, "lit");
        }
        if (lit) {
            lines.push_back(textLine(langCode, "State", tr(langCode, *lit ? "Lit" : "Unlit")));
        }
    }
    if (opt.respawnAnchor) {
        if (auto v = readStateInt(region, pos, "respawn_anchor_charge")) {
            lines.push_back(textLine(langCode, "Charge", std::to_string(*v) + "/4"));
        }
    }

    // ---- pistons ---------------------------------------------------------
    // A piston body has no state of its own, but its block actor tracks the
    // real animation state.
    if (opt.piston) {
        std::optional<PistonState> pistonState;
        if (be && be->getType() == BlockActorType::PistonArm) {
            // mState is a small trivially copyable member, so TypedStorage resolves
            // to PistonState itself (no dereference needed).
            pistonState = static_cast<PistonBlockActor const*>(be)->mState;
        }
        if (pistonState) {
            std::string stateText;
            switch (*pistonState) {
            case PistonState::Expanded:
                stateText = tr(langCode, "extended");
                break;
            case PistonState::Expanding:
                stateText = tr(langCode, "extending");
                break;
            case PistonState::Retracting:
                stateText = tr(langCode, "retracting");
                break;
            default:
                stateText = tr(langCode, "retracted");
                break;
            }
            {
                std::string neighbors;
                for (auto const& offset : {
                         BlockPos{0,  -1, 0 },
                         BlockPos{0,  1,  0 },
                         BlockPos{0,  0,  -1},
                         BlockPos{0,  0,  1 },
                         BlockPos{-1, 0,  0 },
                         BlockPos{1,  0,  0 }
                }) {
                    if (!neighbors.empty()) {
                        neighbors += " ";
                    }
                    neighbors += blockTypeAt(region, pos + offset);
                }
                logOnce(
                    Insight::getInstance().getSelf().getLogger(),
                    "piston",
                    pos,
                    "type=" + type
                        + " actor=" + (be ? std::to_string(static_cast<int>(be->getType())) : std::string("<none>"))
                        + " state=" + std::to_string(static_cast<int>(*pistonState)) + " states=["
                        + describeBlockStateNames(region, pos) + "] neighbors=[" + neighbors + "]"
                );
            }
            lines.push_back(textLine(langCode, "State", stateText));
        } else if (auto facing = readStateInt(region, pos, "facing_direction")) {
            // No piston actor (a client-side copy of a remote world): the extended
            // state is visible as the arm block the piston pushes in front of it.
            // The arm's own block id is the engine data here, so only "extended"
            // can be detected this way - a retracted piston shows no state line.
            auto armType = blockTypeAt(region, pos + facingOffset(*facing));
            if (contains(armType, "piston_arm") || contains(armType, "moving_block")) {
                lines.push_back(textLine(langCode, "State", tr(langCode, "extended")));
            }
        }
    }

    // The arm block itself: Bedrock models it as its own block id
    // (piston_arm_collision / sticky_piston_arm_collision) and exposes neither
    // a state nor a block actor, so the id is the only data available.
    if (contains(type, "piston_arm")) {
        bool sticky = type.find("sticky") != std::string::npos;
        lines.push_back(textLine(langCode, "Piston", sticky ? tr(langCode, "sticky") : tr(langCode, "normal")));
        lines.push_back(textLine(langCode, "State", tr(langCode, "extended")));
    }

    // ---- enchanting table ------------------------------------------------
    // No state involved: the power comes from the bookshelves around it, so the
    // block actor type is what identifies the table.
    if (be && be->getType() == BlockActorType::EnchantingTable) {
        int power = 0;
        for (int dy = 0; dy <= 1; ++dy) {
            for (int dx = -2; dx <= 2; ++dx) {
                for (int dz = -2; dz <= 2; ++dz) {
                    if (dx >= -1 && dx <= 1 && dz >= -1 && dz <= 1) {
                        continue;
                    }
                    BlockPos shelf(pos.x + dx, pos.y + dy, pos.z + dz);
                    if (blockTypeAt(region, shelf) != "minecraft:bookshelf") {
                        continue;
                    }
                    // The block directly between shelf and table must be air
                    // (vanilla requires one free block along the axis the shelf
                    // sits on: |dx| == 2 -> x axis, otherwise the z axis).
                    BlockPos gap = (dx == 2 || dx == -2) ? BlockPos(pos.x + (dx > 0 ? 1 : -1), pos.y + dy, pos.z)
                                                         : BlockPos(pos.x, pos.y + dy, pos.z + (dz > 0 ? 1 : -1));
                    if (isAirAt(region, gap)) {
                        ++power;
                    }
                }
            }
        }
        lines.push_back(valueLine(langCode, "Enchant power", std::min(power, 15)));
    }
}

// ---------------------------------------------------------------------------
// Local cooking estimate.
//
// The engine only details a block entity to a remote client while its container
// screen is open, so everything we can read client side (slots, cooking
// progress, fuel) stops moving the moment that screen is closed. Keep the last
// value the engine gave us and let the clock carry it forward: the estimate is
// exact as long as the machine goes on burning the items that were inside it,
// and it is re-anchored the moment a real value arrives (screen opened again, or
// any change to the slots / to the item being cooked). Fuel running out, or a
// hopper refilling while nobody looks, is the one thing a client cannot see; the
// estimate is bounded by the items that were really in the machine, so it never
// invents a cooking machine that was empty.
struct CookEstimate {
    std::uint64_t                         tag     = 0;
    int                                   perItem = 0;
    int                                   elapsed = 0;
    int                                   queued  = 1;
    std::chrono::steady_clock::time_point anchored{};
    std::chrono::steady_clock::time_point seen{};
};

std::unordered_map<std::uint64_t, CookEstimate>& cookEstimates() {
    static std::unordered_map<std::uint64_t, CookEstimate> estimates;
    return estimates;
}

std::uint64_t cookEstimateKey(void const* source, BlockPos const& pos, int slot) {
    std::uint64_t key = reinterpret_cast<std::uint64_t>(source);
    key               = key * 1000003u + static_cast<std::uint32_t>(pos.x);
    key               = key * 1000003u + static_cast<std::uint32_t>(pos.y);
    key               = key * 1000003u + static_cast<std::uint32_t>(pos.z);
    key               = key * 1000003u + static_cast<std::uint32_t>(slot);
    return key;
}

// `rawElapsed` is the ticks the machine had already done on the item in hand and
// `queued` how many items it had left (that one included), both as of the last
// value the engine sent. Returns false when there is nothing honest left to say:
// nothing was cooking when we last saw a real value, or every item we knew about
// has finished since.
bool advanceCooking(
    void const*     source,
    BlockPos const& pos,
    int             slot,
    std::uint64_t   tag,
    int             perItem,
    int             rawElapsed,
    int             queued,
    int&            outElapsed
) {
    if (perItem <= 0 || rawElapsed <= 0) {
        return false;
    }
    auto&      estimates = cookEstimates();
    auto const now       = std::chrono::steady_clock::now();
    if (estimates.size() > 64) { // nobody keeps every furnace they ever looked at
        for (auto it = estimates.begin(); it != estimates.end();) {
            if (now - it->second.seen > std::chrono::seconds(30)) {
                it = estimates.erase(it);
            } else {
                ++it;
            }
        }
    }
    auto [it, inserted] = estimates.try_emplace(cookEstimateKey(source, pos, slot));
    auto& estimate      = it->second;
    if (inserted || estimate.tag != tag || estimate.perItem != perItem || estimate.elapsed != rawElapsed
        || estimate.queued != queued) {
        if (inserted) { // once per block: what the engine actually reported, for calibration
            logOnce(
                Insight::getInstance().getSelf().getLogger(),
                "cook",
                pos,
                "anchor raw=" + std::to_string(rawElapsed) + "/" + std::to_string(perItem)
                    + " queued=" + std::to_string(queued)
            );
        }
        estimate = CookEstimate{tag, perItem, rawElapsed, std::max(1, queued), now, now};
    } else {
        estimate.seen = now;
    }
    double elapsed = static_cast<double>(estimate.elapsed)
                   + std::chrono::duration<double>(now - estimate.anchored).count() * 20.0; // the server runs at 20 tps
    int    left    = estimate.queued; // items we know were in the machine, the cooking one included
    while (elapsed >= static_cast<double>(perItem)) {
        if (left <= 1) {
            return false; // all of them are done, and we cannot see what came next
        }
        elapsed -= static_cast<double>(perItem);
        --left;
    }
    outElapsed = std::max(1, static_cast<int>(elapsed));
    return true;
}

// Block-actor based extras that are not plain containers.
// Numbers the engine keeps inside the block entity: furnace/brewing-stand
// progress, honey in a bee nest, beacon level, campfire cooking. All of them are
// engine fields, so no block id list is involved; each line obeys the switch of
// the adapter it belongs to.
void blockEntityNumberLines(
    void const*               source,
    BlockPos const&           pos,
    BlockActor const*         be,
    std::string const&        langCode,
    BlockExtrasConfig const&  opt,
    std::vector<std::string>& lines
) {
    if (!be) {
        return;
    }
    switch (be->getType()) {
    case BlockActorType::Furnace:
    case BlockActorType::BlastFurnace:
    case BlockActorType::Smoker: {
        if (!opt.furnace) {
            break;
        }
        auto const* furnace = static_cast<FurnaceBlockActor const*>(be);
        // Only the item currently in the fire is reported. mBurnInterval is how
        // long one item takes in this machine (200 ticks in a furnace, 100 in a
        // blast furnace / smoker) and mCookingProgress how far that item is;
        // mLitTime / mLitDuration describe the fuel and are not used. The item in
        // the input slot also tells us how many are still to come, which is what
        // lets the estimate walk on to the next one after this one is done.
        int const     perItem = static_cast<int>(furnace->mBurnInterval);
        int const     raw     = static_cast<int>(furnace->mCookingProgress);
        int           queued  = 1;
        std::uint64_t tag     = 0;
        if (auto const& input = furnace->getItem(FurnaceBlockActor::SlotIngredient); !input.isNull()) {
            queued = std::max(1, static_cast<int>(input.mCount));
            tag    = std::hash<std::string>{}(input.getDescriptionId());
        }
        int progress = 0;
        if (perItem > 0 && advanceCooking(source, pos, 0, tag, perItem, raw, queued, progress)) {
            // Percent must use this machine's own duration: 200 ticks (10s) in a
            // furnace but 100 (5s) in a blast furnace / smoker, so a fixed
            // divisor would report half the real progress there.
            int const percent = std::clamp(progress * 100 / perItem, 0, 100);
            // rounded up: while anything is still burning the line should not sit
            // on "0s" for a whole second
            int const secondsLeft = (std::max(0, perItem - progress) + 19) / 20;
            lines.push_back(textLine(langCode, "Time left", std::to_string(secondsLeft) + "s"));
            lines.push_back(textLine(langCode, "Cook progress", std::to_string(percent) + "%"));
        }
        break;
    }
    case BlockActorType::BrewingStand: {
        if (!opt.brewing) {
            break;
        }
        auto const* stand = static_cast<BrewingStandBlockActor const*>(be);
        int const   time  = static_cast<int>(stand->mBrewTime); // ticks left of 400
        int         done  = 0;
        if (advanceCooking(source, pos, 0, 0, 400, 400 - time, 1, done)) {
            lines.push_back(textLine(langCode, "Brewing", std::to_string(done / 4) + "%"));
        }
        int const fuel = static_cast<int>(stand->mFuelAmount);
        if (fuel > 0) {
            lines.push_back(textLine(langCode, "Fuel", std::to_string(fuel) + "/" + std::to_string(stand->mFuelTotal)));
        }
        break;
    }
    case BlockActorType::Beehive: {
        if (!opt.misc) {
            break;
        }
        auto const* hive = static_cast<BeehiveBlockActor const*>(be);
        lines.push_back(textLine(langCode, "Bees", std::to_string(hive->mOccupants->size()) + "/3"));
        break;
    }
    case BlockActorType::Beacon: {
        if (!opt.misc) {
            break;
        }
        auto const* beacon = static_cast<BeaconBlockActor const*>(be);
        int const   level  = static_cast<int>(beacon->mNumLevels);
        if (level > 0) {
            lines.push_back(textLine(langCode, "Beacon level", std::to_string(level)));
        }
        break;
    }
    case BlockActorType::Campfire: {
        if (!opt.misc) {
            break;
        }
        auto const* campfire = static_cast<CampfireBlockActor const*>(be);
        for (int slot = 0; slot < 4; ++slot) {
            auto const& item = *campfire->mCookingItem[slot];
            if (item.isNull()) {
                continue;
            }
            // mCookingTime counts the ticks left for that slot (600 = 30 s total)
            int const           left = static_cast<int>(campfire->mCookingTime[slot]);
            std::uint64_t const tag  = std::hash<std::string>{}(item.getDescriptionId());
            int                 done = 0;
            if (!advanceCooking(source, pos, slot, tag, 600, 600 - left, 1, done)) {
                continue;
            }
            lines.push_back(textLine(
                langCode,
                "Cooking",
                localizeKey(langCode, item.getDescriptionId()) + " " + std::to_string((600 - done + 19) / 20) + "s/30s"
            ));
        }
        break;
    }
    default:
        break;
    }
}

void blockActorLines(
    IConstBlockSource const&  region,
    BlockPos const&           pos,
    std::string const&        type,
    std::string const&        langCode,
    BlockActor const*         be,
    std::vector<std::string>& lines,
    BlockExtrasConfig const&  opt
) {
    blockEntityNumberLines(&region, pos, be, langCode, opt, lines);
    (void)region;
    (void)pos;
    (void)type;
    if (!be) {
        return;
    }

    if (opt.banner && be->getType() == BlockActorType::Banner) {
        auto const* banner = static_cast<BannerBlockActor const*>(be);
        // 26.40 turned getPatternCount() into a static that expects a tag; the
        // pattern vector is available on both platforms, so it is read directly.
        int count = static_cast<int>((*banner->mPatterns).size());
        if (count > 0) {
            lines.push_back(valueLine(langCode, "Patterns", count));
        } else {
            lines.push_back(textLine(langCode, "Patterns", tr(langCode, "none")));
        }
        return;
    }
    if (opt.pot && be->getType() == BlockActorType::DecoratedPot) {
        auto const* pot = static_cast<DecoratedPotBlockActor const*>(be);
        // The pot itself is the container (DecoratedPotBlockActor derives from
        // Container) and 26.40 exposes the item range through the actor's own
        // container interface (getItem(0) / mContainedItem) instead.
        if (pot->getContainerSize() > 0) {
            auto const& stack = pot->getItem(0);
            if (stack.isNull()) {
                lines.push_back(textLine(langCode, "Item", tr(langCode, "empty")));
            } else {
                lines.push_back(textLine(
                    langCode,
                    "Item",
                    localizeKey(langCode, stack.getDescriptionId()) + " ×" + std::to_string(stack.mCount)
                ));
            }
        }
        auto const& sherds = *pot->mSherdItemNames; // 26.40: was getSherdNames()
        int         custom = 0;
        for (auto const& s : sherds) {
            if (!s.empty() && s != "minecraft:brick") {
                ++custom;
            }
        }
        {
            int         size    = pot->getContainerSize();
            std::string message = "containerSize=" + std::to_string(size);
            if (size > 0) {
                auto const& stack  = pot->getItem(0);
                message           += " item=" + (stack.isNull() ? std::string("<empty>") : stack.getDescriptionId())
                                   + " count=" + std::to_string(stack.mCount);
            }
            message += " sherds=" + std::to_string(custom);
            logOnce(Insight::getInstance().getSelf().getLogger(), "pot", pos, message);
        }
        lines.push_back(valueLine(langCode, "Sherds", custom) + "/4");
        return;
    }
    if (opt.bookshelf && be->getType() == BlockActorType::ChiseledBookshelf) {
        if (auto* container = containerOf(be)) {
            if (auto list = itemListLine(*container, langCode); !list.empty()) {
                lines.push_back("§7" + tr(langCode, "Bookshelf") + " " + list);
            }
        }
        return;
    }
    if (be->getType() == BlockActorType::Shelf) {
        auto const* shelf = static_cast<ShelfBlockActor const*>(be);
        // isSlotOccupied() is client-only; the container interface is shared.
        int used = 0;
        for (int i = 0; i < shelf->getContainerSize(); ++i) {
            if (!shelf->getItem(i).isNull()) {
                ++used;
            }
        }
        lines.push_back(valueLine(langCode, "Items", used) + "/3");
        if (auto list = itemListLine(*containerOf(be), langCode); !list.empty()) {
            lines.push_back("§7" + tr(langCode, "Shelf") + " " + list);
        }
        return;
    }
    if (opt.itemFrame
        && (be->getType() == BlockActorType::ItemFrame || be->getType() == BlockActorType::GlowItemFrame)) {
        auto const* frame = static_cast<ItemFrameBlockActor const*>(be);
        lines.push_back(textLine(langCode, "Rotation", std::to_string(static_cast<int>(frame->mRotation)) + "°"));
        auto const& item = *frame->mItem; // 26.40: was getFramedItem()
        if (item.isNull()) {
            lines.push_back(textLine(langCode, "Displayed item", tr(langCode, "empty")));
        } else {
            lines.push_back(textLine(langCode, "Displayed item", localizeKey(langCode, item.getDescriptionId())));
        }
        return;
    }
    if (opt.sign && (be->getType() == BlockActorType::Sign || be->getType() == BlockActorType::HangingSign)) {
        auto const* sign = static_cast<SignBlockActor const*>(be);
        // the panel keeps one line per entry, so the sign's own line breaks
        // become separators instead of splitting the panel
        for (int side = 0; side < 2; ++side) {
            std::string text;
            for (char const ch : sign->getMessage(side == 0 ? SignTextSide::Front : SignTextSide::Back)) {
                if (ch == '\n' || ch == '\r') {
                    if (!text.empty()) {
                        text += " §8| §f";
                    }
                } else {
                    text += ch;
                }
            }
            if (!text.empty()) {
                lines.push_back("§7" + tr(langCode, side == 0 ? "Text" : "Text (back)") + " §f" + text);
            }
        }
        return;
    }
    if (opt.lectern && be->getType() == BlockActorType::Lectern) {
        auto const* lectern = static_cast<LecternBlockActor const*>(be);
        if ((*lectern->mBook).isNull()) { // 26.40: was hasBook()
            lines.push_back(textLine(langCode, "Book", tr(langCode, "none")));
        } else {
            std::string name = firstItemName(be, langCode);
            lines.push_back(textLine(langCode, "Book", name.empty() ? tr(langCode, "has") : name));
            int         page     = static_cast<int>(lectern->mPage);       // 26.40: was getPage()
            int         total    = static_cast<int>(lectern->mTotalPages); // 26.40: was getTotalPages()
            std::string pageText = std::to_string(page + 1);
            if (total > 0) {
                pageText += "/" + std::to_string(total);
            }
            lines.push_back(textLine(langCode, "Page", pageText));
        }
        return;
    }
}

} // namespace

// Exported wrapper around the file-local containerOf(): the client's live-snapshot
// code needs the same lookup to recognise a screen's block container model.
Container const* blockContainerOf(BlockActor const* actor) { return containerOf(actor); }

std::string describeBlockFacing(IConstBlockSource const& region, BlockPos const& pos, std::string const& langCode) {
    if (auto v = readStateInt(region, pos, "facing_direction")) {
        return facingWord(langCode, *v);
    }
    if (auto v = readStateIntAny(region, pos, {"direction", "weirdo_direction"})) {
        return compassWord(langCode, *v);
    }
    // Doors, chests, barrels, ... use the namespaced string-enum state
    // "minecraft:cardinal_direction" (Direction::Type: South=0 West=1
    // North=2 East=3), which shares the compass mapping above.
    if (auto v = readStateIntAny(region, pos, {"minecraft:cardinal_direction", "cardinal_direction"})) {
        return compassWord(langCode, *v);
    }
    if (auto v = readStateIntAny(region, pos, {"ground_sign_direction", "rotation"})) {
        // 16-step rotation: 0 = south, 4 = west, 8 = north, 12 = east
        return compassWord(langCode, (*v / 4) % 4);
    }
    if (auto v = readStateIntAny(region, pos, {"pillar_axis", "axis"})) {
        switch (*v) {
        case 0:
            return tr(langCode, "x");
        case 1:
            return tr(langCode, "y");
        case 2:
            return tr(langCode, "z");
        default:
            return {};
        }
    }
    return {};
}

std::string describeBlockLight(IConstBlockSource const& region, BlockPos const& pos) {
    try {
        float brightness = region.getBrightness(pos); // 0..1
        int   level      = static_cast<int>(brightness * 15.0f + 0.5f);
        if (level < 0) {
            level = 0;
        }
        if (level > 15) {
            level = 15;
        }
        return std::to_string(level);
    } catch (...) {
        return {};
    }
}

std::string describeBlockEmission(IConstBlockSource const& region, BlockPos const& pos) {
    try {
        auto const& block = region.getBlock(pos);
        auto const& type  = block.getBlockType();
        auto        value = type.getLightEmission(block); // virtual, 0..15
        int         level = static_cast<int>(value.mValue);
        if (level < 0) {
            level = 0;
        }
        if (level > 15) {
            level = 15;
        }
        return std::to_string(level);
    } catch (...) {
        return {};
    }
}

void setLiveContainerSnapshot(BlockPos const& pos, int filled, int total, int totalCount) {
    auto& snapshots = liveSnapshots();
    if (snapshots.size() > 512) {
        snapshots.clear(); // defensive: never grow without bound
    }
    snapshots[std::make_tuple(pos.x, pos.y, pos.z)] = ContainerSnapshot{filled, total, totalCount};
}

void setLiveEntityContainerSnapshot(int64_t actorId, int filled, int total, int totalCount) {
    auto& snapshots = liveEntitySnapshots();
    if (snapshots.size() > 512) {
        snapshots.clear();
    }
    snapshots[actorId] = ContainerSnapshot{filled, total, totalCount};
}

std::string describeBlockStateNames(IConstBlockSource const& region, BlockPos const& pos) {
    try {
        auto const& block = region.getBlock(pos);
        auto const& type  = block.getBlockType();
        std::string out;
        for (auto const& [name, stateId] : *type.mStateNameMap) {
            auto value = block.getState<int>(stateId);
            if (!out.empty()) {
                out += ",";
            }
            out += name.getString();
            if (value) {
                out += "=" + std::to_string(*value);
            }
        }
        return out;
    } catch (...) {
        return {};
    }
}

std::string buildBlockExtras(
    IConstBlockSource const& region,
    BlockPos const&          pos,
    std::string const&       typeName,
    std::string const&       langCode,
    BlockExtrasConfig const& opt
) {
    if (!opt.enabled) {
        return {};
    }
    std::vector<std::string> lines;

    // ---- data every block carries ----------------------------------------
    // Breaking time and explosion resistance come straight from the block type;
    // flame odds only exist for blocks with a flammable component.
    {
        auto const& block     = region.getBlock(pos);
        auto const& blockType = block.getBlockType();
        if (opt.hardness) {
            lines.push_back(textLine(langCode, "Breaking time", util::trimNumber(blockType.getDestroySpeed(), 2)));
        }
        if (opt.blastResistance) {
            lines.push_back(
                textLine(langCode, "Explosion resistance", util::trimNumber(blockType.getExplosionResistance(), 2))
            );
        }
        // TODO(26.40 + LeviLamina): "chance to catch fire" is not readable yet.
        // BlockType exposes no flame/burn/odds member, BlockProperty has no
        // flammability flag, and BlockFlammableComponent cannot be reached -
        // hasComponent<BlockFlammableComponent>() answers true but the typed
        // pointer lands on unrelated block-type floats (3.0 / 30.0 / 4.0 came back
        // as 16448 / 16880 / 16512), and that header even disagrees with
        // BlockFlammableDescription about the field width (2-byte FlameOdds vs
        // 4-byte int). Re-add the line and the extras.igniteChance switch once
        // something upstream makes the data reachable.
    }

    BlockActor const* be        = nullptr;
    bool const        wantActor = opt.chest || opt.bookshelf || opt.shelf || opt.lectern || opt.pot || opt.furnace
                               || opt.brewing || opt.sign || opt.banner || opt.itemFrame || opt.flowerPot || opt.jukebox
                               || opt.redstone || opt.comparator || opt.dispenser || opt.repeater || opt.candle
                               || opt.respawnAnchor || opt.piston || opt.misc;
    if (wantActor) {
        be = region.getBlockEntity(pos);
    }

    if (opt.chest && be && containerOf(be) && isContainerBlock(region, pos)) {
        auto line = chestLine(be, pos, langCode);
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    // machine slots: the block actor type says whether this is a furnace-like
    // block or a brewing stand (engine enum, not a list of block ids)
    if (opt.furnace && be && containerOf(be)
        && (be->getType() == BlockActorType::Furnace || be->getType() == BlockActorType::BlastFurnace
            || be->getType() == BlockActorType::Smoker)) {
        auto line = machineLine(be, pos, langCode);
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    if (opt.brewing && be && containerOf(be) && be->getType() == BlockActorType::BrewingStand) {
        auto line = machineLine(be, pos, langCode);
        if (!line.empty()) {
            lines.push_back(line);
        }
    }

    if (opt.redstone) {
        // Anything that carries a redstone level (wire, pressure plate, target,
        // repeater, ...) reports it as a block state, so no block id list is
        // needed; a comparator additionally has its own switch below.
        if (auto v = readStateIntAny(region, pos, {"output_signal", "redstone_signal"})) {
            lines.push_back(valueLine(langCode, "Signal", *v));
        }
    }
    if (opt.comparator && be && be->getType() == BlockActorType::Comparator) {
        auto line = comparatorLine(region, pos, be, langCode);
        if (!line.empty()) {
            lines.push_back(line);
        }
    }

    // block-state lines (misc is the master switch for them; the individual
    // switches below refine it)
    if (opt.misc) {
        miscStateLines(region, pos, typeName, langCode, lines, opt);
    }
    // block entities: blockActorLines() gates every branch by its own switch, so
    // this call has to happen whenever any of those switches is on. A switch left
    // out here silently takes the lines it owns down with it - turning banners off
    // used to silence the furnace readout and the campfire timers too.
    if (opt.banner || opt.pot || opt.shelf || opt.bookshelf || opt.lectern || opt.itemFrame || opt.sign || opt.furnace
        || opt.brewing || opt.misc) {
        blockActorLines(region, pos, typeName, langCode, be, lines, opt);
    }

    {
        // 26.40 uses BlockActorType::Jukebox for the block, older data says
        // Music; both are containers holding the disc in slot 0.
        if (opt.jukebox && be && (be->getType() == BlockActorType::Music || be->getType() == BlockActorType::Jukebox)) {
            if (auto* c = containerOf(be); c && c->getContainerSize() > 0) {
                auto const& item = c->getItem(0);
                if (item.isNull()) {
                    lines.push_back("§7" + tr(langCode, "Record") + " §8-");
                } else {
                    // The engine carries one localization key per disc
                    // (item.record_<variant>.desc), so the disc's own name comes
                    // from the game's tables - including resource packs. Only the
                    // variant has to be worked out: modern items encode it in the
                    // raw id (minecraft:record_cat), legacy ones in the aux value.
                    std::string name = localizeKey(langCode, item.getDescriptionId());
                    auto const  raw  = item.getRawNameId();
                    // The item itself knows which disc it is (minecraft:record_cat);
                    // the stack-level raw id stays the generic "minecraft:record".
                    std::string serialized;
                    if (item.mItem) {
                        serialized = item.mItem->getSerializedName();
                    }
                    // Modern Bedrock names discs minecraft:music_disc_cat, older
                    // data uses minecraft:record_cat; the localization key is
                    // item.record_<variant>.desc in both cases.
                    static constexpr char const* kDiscPrefixes[] = {"minecraft:music_disc_", "minecraft:record_"};
                    std::string                  variant;
                    for (auto const* prefix : kDiscPrefixes) {
                        auto const length = std::strlen(prefix);
                        if (serialized.rfind(prefix, 0) == 0) {
                            variant = serialized.substr(length);
                            break;
                        }
                        if (raw.rfind(prefix, 0) == 0) {
                            variant = raw.substr(length);
                            break;
                        }
                    }
                    if (variant.empty()) {
                        if (int const aux = item.getAuxValue(); aux >= 0 && aux < 12) {
                            static constexpr char const* kLegacyDiscVariants[] = {
                                "13",
                                "cat",
                                "blocks",
                                "chirp",
                                "far",
                                "mall",
                                "mellohi",
                                "stal",
                                "strad",
                                "ward",
                                "11",
                                "wait"
                            };
                            variant = kLegacyDiscVariants[aux];
                        }
                    }
                    if (!variant.empty()) {
                        std::string const disc = localizeKey(langCode, "item.record_" + variant + ".desc");
                        if (disc.rfind("item.record_", 0) != 0) {
                            name = disc; // resolved by the engine
                        } else {
                            name += " (" + variant + ")"; // no entry for it here
                        }
                    }
                    logOnce(
                        Insight::getInstance().getSelf().getLogger(),
                        "record",
                        pos,
                        "id=" + item.getDescriptionId() + " raw=" + raw + " serialized=" + serialized
                            + " idAux=" + std::to_string(item.getIdAux()) + " aux=" + std::to_string(item.getAuxValue())
                            + " shown=" + name
                    );
                    lines.push_back("§7" + tr(langCode, "Record") + " §f" + name);
                }
            }
        } else if (opt.flowerPot && be && be->getType() == BlockActorType::FlowerPot) {
            auto const* fp = static_cast<FlowerPotBlockActor const*>(be);
            // 26.40: getPlantItem() was replaced by the mPlant member.
            auto const* plant = static_cast<Block const*>(fp->mPlant);
            if (plant) {
                lines.push_back("§7" + tr(langCode, "Pot") + " §f" + localizeKey(langCode, plant->getDescriptionId()));
            }
        } else if (opt.flowerPot && isPottedBlock(region, pos)) {
            // Modern versions encode the plant in the block itself. Bedrock
            // exposes no flag for "this block is a potted plant", so the block's
            // own display name (which is where the plant lives) is used - the
            // engine data, without parsing the id.
            lines.push_back(
                "§7" + tr(langCode, "Pot") + " §f" + localizeKey(langCode, region.getBlock(pos).getDescriptionId())
            );
        }
    }

    std::string out;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i) {
            out += '\n';
        }
        out += lines[i];
    }
    return out;
}

std::string buildEntityExtras(
    IConstBlockSource const& region,
    Actor const&             actor,
    std::string const&       langCode,
    BlockExtrasConfig const& opt
) {
    // kept for signature symmetry with buildBlockExtras(); entity extras only
    // read actor components, so no block sampling happens here.
    (void)region;
    if (!opt.enabled) {
        return {};
    }
    std::vector<std::string> lines;

    // ---- container entities: whatever carries a container component ------
    // (chest/hopper minecart, boat with chest, ...). The component is the
    // engine data, so no list of entity ids is needed.
    if (opt.chest) {
        int size   = 0;
        int filled = 0;
        int total  = 0;
        if (auto component = actor.getEntityContext().tryGetComponent<ContainerComponent>()) {
            // 26.40: the component no longer forwards the container interface
            // itself, it holds the FillingContainer that does.
            auto const& container = *component->mContainer;
            size                  = container.getContainerSize();
            for (int i = 0; i < size; ++i) {
                auto const& item = container.getItem(i);
                if (!item.isNull()) {
                    ++filled;
                    total += item.mCount;
                }
            }
        }
        // Entity containers have the same staleness problem as block ones: the
        // component keeps its old contents, while the container UI shows live
        // ones. Prefer the snapshot captured from the open UI.
        auto& snapshots = liveEntitySnapshots();
        auto  key       = static_cast<int64_t>(actor.getOrCreateUniqueID().rawID);
        if (auto it = snapshots.find(key); it != snapshots.end()) {
            auto const& snap = it->second;
            if (snap.total == size && snap.filled == filled && snap.totalCount == total) {
                snapshots.erase(it);
            } else {
                filled = snap.filled;
                size   = snap.total;
                total  = snap.totalCount;
            }
        }
        if (size > 0) {
            std::string s = "§7" + tr(langCode, "Items") + " §f" + std::to_string(filled) + "/" + std::to_string(size);
            if (total > 0) {
                s += " §7· §f" + std::to_string(total);
            }
            lines.push_back(s);
        }
    }

    // ---- paintings -------------------------------------------------------
    // A painting is an entity whose motive says which artwork it is; the motive
    // name is engine data, so no list of painting ids is involved.
    if (opt.painting && actor.isType(ActorType::Painting)) {
        auto const* painting = static_cast<Painting const*>(&actor);
        if (painting->mMotif) {
            auto const& name = *painting->mMotif->mName;
            if (!name.empty()) {
                lines.push_back("§7" + tr(langCode, "Painting") + " §f" + name);
            }
        }
    }

    // ---- worn equipment --------------------------------------------------
    // The engine exposes the equipment slots themselves, so whatever actually
    // carries an item there reports it (armor stands, players, armored mobs) -
    // no entity id check is involved. Entities without equipment return empty
    // stacks and simply contribute nothing.
    if (opt.misc) {
        struct SlotLabel {
            SharedTypes::Legacy::ArmorSlot slot;
            char const*                    labelKey;
        };
        static constexpr SlotLabel slots[] = {
            {SharedTypes::Legacy::ArmorSlot::Head,  "Helmet"    },
            {SharedTypes::Legacy::ArmorSlot::Torso, "Chestplate"},
            {SharedTypes::Legacy::ArmorSlot::Legs,  "Leggings"  },
            {SharedTypes::Legacy::ArmorSlot::Feet,  "Boots"     },
        };
        try {
            for (auto const& s : slots) {
                auto const& item = actor.getArmor(s.slot);
                if (!item.isNull()) {
                    lines.push_back(textLine(langCode, s.labelKey, localizeKey(langCode, item.getDescriptionId())));
                }
            }
            auto const& mainHand = actor.getCarriedItem();
            if (!mainHand.isNull()) {
                lines.push_back(textLine(langCode, "Main hand", localizeKey(langCode, mainHand.getDescriptionId())));
            }
            auto const& offHand = actor.getOffhandSlot();
            if (!offHand.isNull()) {
                lines.push_back(textLine(langCode, "Off hand", localizeKey(langCode, offHand.getDescriptionId())));
            }
        } catch (...) {
            // actors without an equipment inventory simply report nothing
        }
    }

    // ---- {light}/{direction} are separate placeholders: they are not
    //      repeated here (see describeBlockLight / describeBlockFacing) ----

    std::string out;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i) {
            out += '\n';
        }
        out += lines[i];
    }
    return out;
}

} // namespace insight
