#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "Config.h"

#include "mc/deps/vanilla_components/IConstBlockSource.h"
#include "mc/world/level/BlockPos.h"

// Global forward declaration: the entity extras take the engine's Actor.
class Actor;
class BlockActor;
class Container;

namespace insight {

// One non-empty container slot, in the form the client panel needs to draw it with the
// game's item renderer. Kept to plain data rather than an ItemStack: the extras are
// built on both platforms, and only the client ever turns these into icons.
struct ContainerItem {
    std::string name;        // full item name, e.g. "minecraft:stone"
    int         count = 0;   // stack size
    int         aux   = 0;   // aux value: the variant a name alone does not carry

    // Lists of these are compared to decide whether a snapshot has been caught up with and
    // whether the panel's icon stacks have to be rebuilt.
    [[nodiscard]] bool operator==(ContainerItem const& other) const = default;
};

// A container's contents as the client panel draws them: which line of the extras block
// they belong to, and the stacks themselves. Filled by the same builders that write the
// "Items n/m" line, from the same source (including the live copy captured while a
// container screen is open), so text and icons can never disagree.
struct ContainerContents {
    int                        index = -1; // line inside the extras block, -1 = none
    std::vector<ContainerItem> items;
};

// Builds the extra, per-block-type information lines for the block at `pos`.
// Returns one or more "\n"-joined lines (without a trailing newline), or an
// empty string when nothing applies. Labels are translated for `langCode`
// through the mod's message catalogue (lang/<locale>.json, see I18n.h); block
// and item *names* come from the game's own translation tables, and the raw
// translation key is shown when no localized name exists. When `contents` is
// given, a container's items are reported there as well.
[[nodiscard]] std::string buildBlockExtras(
    IConstBlockSource const& region,
    BlockPos const&          pos,
    std::string const&       typeName,
    std::string const&       langCode,
    BlockExtrasConfig const& opt,
    ContainerContents*       contents = nullptr
);

// Facing / orientation of the block at `pos` for the {direction} placeholder.
// Returns a localized word ("北" / "north", "上" / "up", ...), or an empty
// string when the block has no facing state we understand.
[[nodiscard]] std::string
describeBlockFacing(IConstBlockSource const& region, BlockPos const& pos, std::string const& langCode);

// Light level at `pos` (0..15) for the {light} placeholder.
[[nodiscard]] std::string describeBlockLight(IConstBlockSource const& region, BlockPos const& pos);

// Light emitted by the block itself (0..15) for the {emission} placeholder.
[[nodiscard]] std::string describeBlockEmission(IConstBlockSource const& region, BlockPos const& pos);

// Compass facing of an entity's own yaw, for the name line of the entity panel.
// Returns a localized word ("北" / "north", ...), or an empty string when the
// rotation cannot be read.
[[nodiscard]] std::string describeEntityFacing(Actor const& actor, std::string const& langCode);

// Live container snapshot, client only. While a container UI is open the
// engine keeps an up-to-date container for the UI, whereas the world copy we
// otherwise read stays stale until the chunk is reloaded. The client captures
// that live container here (keyed by block position) and it is preferred until
// the world copy catches up. `size` is the container's slot count, which the
// non-empty `items` alone do not tell.
void setLiveContainerSnapshot(BlockPos const& pos, std::vector<ContainerItem> const& items, int size);

// Same as above for entity containers (chest/hopper minecart, boat with
// chest), keyed by the actor's unique id.
void setLiveEntityContainerSnapshot(int64_t actorId, std::vector<ContainerItem> const& items, int size);

// Forgets every cache keyed by a block position, an actor id or an address in
// the level's memory: the container snapshots and the cooking estimates. A
// world reload reuses coordinates and pointers, so a leftover entry would be
// read as the new world's own data. Call it whenever the level changes.
void resetRuntimeState();

// Container a block actor owns (26.40 keeps it on the actor's main component).
// Exported because the client's live-snapshot code needs the same lookup to
// recognise which of a screen's container models belongs to the block.
[[nodiscard]] Container const* blockContainerOf(BlockActor const* actor);

// Debug helper: "state=value,state=value" for every state of the block at
// `pos` (used by the debug log to calibrate state names).
[[nodiscard]] std::string describeBlockStateNames(IConstBlockSource const& region, BlockPos const& pos);

// Extra lines for the entity under the crosshair: container minecarts / boats
// ("物品 n/m"), paintings, the worn equipment, and the entity adapters of
// EntityExtrasConfig (owner, status effects, breeding timer, conversion time, pet
// armour, mount numbers, an armour stand's pose). {light}/{direction} are separate
// placeholders and are not repeated here. `opt.enabled` gates every adapter, block
// and entity alike. When `contents` is given, a container entity's items are
// reported there as well.
[[nodiscard]] std::string buildEntityExtras(
    IConstBlockSource const&  region,
    Actor const&              actor,
    std::string const&        langCode,
    BlockExtrasConfig const&  opt,
    EntityExtrasConfig const& entityOpt,
    ContainerContents*        contents = nullptr
);

} // namespace insight
