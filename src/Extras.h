#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "Config.h"

#include "mc/deps/vanilla_components/IConstBlockSource.h"
#include "mc/world/level/BlockPos.h"

// Global forward declaration: the entity extras take the engine's Actor.
class Actor;
class BlockActor;
class Container;

namespace insight {

// Builds the extra, per-block-type information lines for the block at `pos`.
// Returns one or more "\n"-joined lines (without a trailing newline), or an
// empty string when nothing applies. Labels are translated for `langCode`
// through the mod's message catalogue (lang/<locale>.json, see I18n.h); block
// and item *names* come from the game's own translation tables, and the raw
// translation key is shown when no localized name exists.
[[nodiscard]] std::string buildBlockExtras(
    IConstBlockSource const& region,
    BlockPos const&          pos,
    std::string const&       typeName,
    std::string const&       langCode,
    BlockExtrasConfig const& opt
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

// Live container snapshot, client only. While a container UI is open the
// engine keeps an up-to-date container for the UI, whereas the world copy we
// otherwise read stays stale until the chunk is reloaded. The client captures
// that live container here (keyed by block position) and it is preferred until
// the world copy catches up.
void setLiveContainerSnapshot(BlockPos const& pos, int filled, int total, int totalCount);

// Same as above for entity containers (chest/hopper minecart, boat with
// chest), keyed by the actor's unique id.
void setLiveEntityContainerSnapshot(int64_t actorId, int filled, int total, int totalCount);

// Container a block actor owns (26.40 keeps it on the actor's main component).
// Exported because the client's live-snapshot code needs the same lookup to
// recognise which of a screen's container models belongs to the block.
[[nodiscard]] Container const* blockContainerOf(BlockActor const* actor);

// Debug helper: "state=value,state=value" for every state of the block at
// `pos` (used by the debug log to calibrate state names).
[[nodiscard]] std::string describeBlockStateNames(IConstBlockSource const& region, BlockPos const& pos);

// Extra lines for the entity under the crosshair: container minecarts / boats
// ("物品 n/m") and armor stand equipment. {light}/{direction} are separate
// placeholders and are not repeated here.
[[nodiscard]] std::string buildEntityExtras(
    IConstBlockSource const& region,
    Actor const&             actor,
    std::string const&       langCode,
    BlockExtrasConfig const& opt
);

} // namespace insight
