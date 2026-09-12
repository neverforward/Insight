#include "Extras.h"

#include <array>
#include <cmath>
#include <cstring>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "mc/deps/core/string/HashedString.h"
#include "mc/deps/shared_types/legacy/actor/ArmorSlot.h"
#include "mc/entity/components_json_legacy/ContainerComponent.h"
#include "mc/world/Container.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/item/ItemStackBase.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockType.h"
#include "mc/world/level/block/actor/BannerBlockActor.h"
#include "mc/world/level/block/actor/BlockActor.h"
#include "mc/world/level/block/actor/BlockActorType.h"
#include "mc/world/level/block/actor/ComparatorBlockActor.h"
#include "mc/world/level/block/actor/DecoratedPotBlockActor.h"
#include "mc/world/level/block/actor/FlowerPotBlockActor.h"
#include "mc/world/level/block/actor/ItemFrameBlockActor.h"
#include "mc/world/level/block/actor/LecternBlockActor.h"
#include "mc/world/level/block/actor/PistonBlockActor.h"
#include "mc/world/level/block/actor/PistonState.h"
#include "mc/world/level/block/actor/ShelfBlockActor.h"

#include "ll/api/io/Logger.h"

#include "I18n.h"
#include "Insight.h"
#include "Translation.h"

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
std::string chestLine(BlockActor const* be, BlockPos const& pos, std::string const& langCode) {
    if (!be) {
        return {};
    }
    auto* c = be->getContainer();
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

    std::string s = "§7" + tr(langCode, "Items") + " §f" + std::to_string(filled) + "/"
                  + std::to_string(size);
    if (total > 0) {
        s += " §7· §f" + std::to_string(total);
    }
    return s;
}

std::string machineLine(BlockActor const* be, BlockPos const& pos, std::string const& langCode) {
    if (!be) {
        return {};
    }
    auto* c = be->getContainer();
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
    return "§7" + tr(langCode, "Slots") + " §f" + std::to_string(filled) + "/"
         + std::to_string(size);
}

// First non-empty slot of a container as a localized item name ("" if empty).
std::string firstItemName(BlockActor const* be, std::string const& langCode) {
    if (!be) {
        return {};
    }
    auto* c = be->getContainer();
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
        return region.getBlock(pos).isContainerBlock();
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
// comparator: prefer the block-entity signal, fall back to the block state
// ---------------------------------------------------------------------------
std::string comparatorLine(
    IConstBlockSource const& region,
    BlockPos const&          pos,
    BlockActor const*        be,
    std::string const&       langCode
) {
    if (be && be->isType(BlockActorType::Comparator)) {
        int signal = const_cast<ComparatorBlockActor*>(static_cast<ComparatorBlockActor const*>(be))->getOutputSignal();
        return valueLine(langCode, "Signal", signal);
    }
    if (auto v = readStateIntAny(region, pos, {"output_signal", "powered"})) {
        return valueLine(langCode, "Signal", *v);
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
    std::vector<std::string>& lines
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
    if (auto v = readStateBool(region, pos, "powered_bit")) { // observers, tripwire hooks, ...
        lines.push_back(textLine(langCode, "Powered", tr(langCode, *v ? "yes" : "no")));
    }
    if (auto v = readStateBool(region, pos, "crafting")) { // crafter
        lines.push_back(textLine(langCode, "State", tr(langCode, *v ? "crafting" : "idle")));
    }
    if (auto v = readStateBool(region, pos, "triggered")) { // crafter
        if (*v) {
            lines.push_back(textLine(langCode, "Triggered", tr(langCode, "yes")));
        }
    }

    // ---- numeric states --------------------------------------------------
    if (auto v = readStateInt(region, pos, "disabled_slots_bit")) { // crafter
        int disabled = 0;
        for (int bit = 0; bit < 9; ++bit) {
            if ((*v >> bit) & 1) {
                ++disabled;
            }
        }
        if (disabled > 0) {
            lines.push_back(valueLine(langCode, "Disabled slots", disabled));
        }
    }
    if (auto v = readStateInt(region, pos, "composter_fill_level")) {
        lines.push_back("§7" + tr(langCode, "Compost") + " §f" + std::to_string(*v) + "/8");
    }
    if (auto v = readStateInt(region, pos, "bite_counter")) { // cake
        lines.push_back("§7" + tr(langCode, "Slices") + " §f" + std::to_string(7 - *v) + "/7");
    }
    if (auto v = readStateInt(region, pos, "note")) { // note block
        lines.push_back("§7" + tr(langCode, "Note") + " §f" + std::to_string(*v + 1) + "/25");
    }
    if (auto v = readStateInt(region, pos, "cluster_count")) { // sea pickle
        lines.push_back(valueLine(langCode, "Count", *v + 1));
    }

    // ---- pistons ---------------------------------------------------------
    // A piston body has no state of its own, but its block actor tracks the
    // real animation state.
    std::optional<PistonState> pistonState;
    if (be && be->isType(BlockActorType::PistonArm)) {
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
            for (auto const& offset : {BlockPos{0, -1, 0}, BlockPos{0, 1, 0}, BlockPos{0, 0, -1},
                                       BlockPos{0, 0, 1}, BlockPos{-1, 0, 0}, BlockPos{1, 0, 0}}) {
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
                    + " state=" + std::to_string(static_cast<int>(*pistonState))
                    + " states=[" + describeBlockStateNames(region, pos) + "] neighbors=[" + neighbors + "]"
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
    if (be && be->isType(BlockActorType::EnchantingTable)) {
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

// Block-actor based extras that are not plain containers.
void blockActorLines(
    IConstBlockSource const&  region,
    BlockPos const&           pos,
    std::string const&        type,
    std::string const&        langCode,
    BlockActor const*         be,
    std::vector<std::string>& lines
) {
    (void)region;
    (void)pos;
    (void)type;
    if (!be) {
        return;
    }

    if (be->isType(BlockActorType::Banner)) {
        auto const* banner = static_cast<BannerBlockActor const*>(be);
        // getPatternCount() is client-only in the SDK headers; the pattern
        // vector itself is available on both platforms.
#ifdef LL_PLAT_C
        int count = banner->getPatternCount();
#else
        int count = static_cast<int>((*banner->mPatterns).size());
#endif
        if (count > 0) {
            lines.push_back(valueLine(langCode, "Patterns", count));
        } else {
            lines.push_back(textLine(langCode, "Patterns", tr(langCode, "none")));
        }
        return;
    }
    if (be->isType(BlockActorType::DecoratedPot)) {
        auto const* pot = static_cast<DecoratedPotBlockActor const*>(be);
        // The pot itself is the container (DecoratedPotBlockActor derives from
        // Container) - BlockActor::getContainer() is NOT overridden for it, so
        // the item has to be read through the actor's own container interface
        // (getItem(0) / mContainedItem) instead.
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
        auto const& sherds = pot->getSherdNames();
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
                auto const& stack = pot->getItem(0);
                message += " item=" + (stack.isNull() ? std::string("<empty>") : stack.getDescriptionId())
                         + " count=" + std::to_string(stack.mCount);
            }
            message += " sherds=" + std::to_string(custom);
            logOnce(Insight::getInstance().getSelf().getLogger(), "pot", pos, message);
        }
        lines.push_back(valueLine(langCode, "Sherds", custom) + "/4");
        return;
    }
    if (be->isType(BlockActorType::Shelf)) {
        auto const* shelf = static_cast<ShelfBlockActor const*>(be);
        // isSlotOccupied() is client-only; the container interface is shared.
        int used = 0;
        for (int i = 0; i < shelf->getContainerSize(); ++i) {
            if (!shelf->getItem(i).isNull()) {
                ++used;
            }
        }
        lines.push_back(valueLine(langCode, "Items", used) + "/3");
        return;
    }
    if (be->isType(BlockActorType::ItemFrame) || be->isType(BlockActorType::GlowItemFrame)) {
        auto const* frame = static_cast<ItemFrameBlockActor const*>(be);
        auto const& item  = frame->getFramedItem();
        if (item.isNull()) {
            lines.push_back(textLine(langCode, "Displayed item", tr(langCode, "empty")));
        } else {
            lines.push_back(textLine(langCode, "Displayed item", localizeKey(langCode, item.getDescriptionId())));
        }
        return;
    }
    if (be->isType(BlockActorType::Lectern)) {
        auto const* lectern = static_cast<LecternBlockActor const*>(be);
        if (!lectern->hasBook()) {
            lines.push_back(textLine(langCode, "Book", tr(langCode, "none")));
        } else {
            std::string name = firstItemName(be, langCode);
            lines.push_back(textLine(langCode, "Book", name.empty() ? tr(langCode, "has") : name));
            int         page     = lectern->getPage();
            int         total    = lectern->getTotalPages();
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

    BlockActor const* be = nullptr;
    if (opt.chest || opt.furnace || opt.brewing || opt.redstone || opt.misc) {
        be = region.getBlockEntity(pos);
    }

    if (opt.chest && be && be->getContainer() && isContainerBlock(region, pos)) {
        auto line = chestLine(be, pos, langCode);
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    // machine slots: the block actor type says whether this is a furnace-like
    // block or a brewing stand (engine enum, not a list of block ids)
    if (opt.furnace && be && be->getContainer()
        && (be->isType(BlockActorType::Furnace) || be->isType(BlockActorType::BlastFurnace)
            || be->isType(BlockActorType::Smoker))) {
        auto line = machineLine(be, pos, langCode);
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    if (opt.brewing && be && be->getContainer() && be->isType(BlockActorType::BrewingStand)) {
        auto line = machineLine(be, pos, langCode);
        if (!line.empty()) {
            lines.push_back(line);
        }
    }

    if (opt.redstone) {
        // A comparator reports through its block actor, everything else that
        // carries a redstone level (repeater, wire, pressure plate, target)
        // reports it as a block state - so neither branch needs a block id list.
        if (be && be->isType(BlockActorType::Comparator)) {
            auto line = comparatorLine(region, pos, be, langCode);
            if (!line.empty()) {
                lines.push_back(line);
            }
        } else if (auto v = readStateIntAny(region, pos, {"output_signal", "redstone_signal"})) {
            lines.push_back(valueLine(langCode, "Signal", *v));
        }
    }

    if (opt.misc) {
        blockActorLines(region, pos, typeName, langCode, be, lines);
        miscStateLines(region, pos, typeName, langCode, lines);

        if (be && be->isType(BlockActorType::Music)) { // jukebox
            if (auto* c = be->getContainer(); c && c->getContainerSize() > 0) {
                auto const& item = c->getItem(0);
                if (item.isNull()) {
                    lines.push_back("§7" + tr(langCode, "Record") + " §8-");
                } else {
                    lines.push_back(
                        "§7" + tr(langCode, "Record") + " §f" + localizeKey(langCode, item.getDescriptionId())
                    );
                }
            }
        } else if (be && be->isType(BlockActorType::FlowerPot)) {
            auto const* fp    = static_cast<FlowerPotBlockActor const*>(be);
            auto const* plant = fp->getPlantItem();
            if (plant) {
                lines.push_back(
                    "§7" + tr(langCode, "Pot") + " §f" + localizeKey(langCode, plant->getDescriptionId())
                );
            }
        } else if (isPottedBlock(region, pos)) {
            // Modern versions encode the plant in the block itself. Bedrock
            // exposes no flag for "this block is a potted plant", so the block's
            // own display name (which is where the plant lives) is used - the
            // engine data, without parsing the id.
            lines.push_back(
                "§7" + tr(langCode, "Pot") + " §f"
                + localizeKey(langCode, region.getBlock(pos).getDescriptionId())
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
        if (auto container = actor.getEntityContext().tryGetComponent<ContainerComponent>()) {
            size = container->getContainerSize();
            for (int i = 0; i < size; ++i) {
                auto const& item = container->getItem(i);
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
            std::string s = "§7" + tr(langCode, "Items") + " §f" + std::to_string(filled) + "/"
                          + std::to_string(size);
            if (total > 0) {
                s += " §7· §f" + std::to_string(total);
            }
            lines.push_back(s);
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
                lines.push_back(
                    textLine(langCode, "Main hand", localizeKey(langCode, mainHand.getDescriptionId()))
                );
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
