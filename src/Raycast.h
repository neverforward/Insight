#pragma once

#include <optional>
#include <string>

#include "mc/deps/core/math/Vec3.h"
#include "mc/deps/vanilla_components/IConstBlockSource.h"
#include "mc/math/vector/Vecs.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/block/Block.h"

namespace insight {

struct RaycastResult {
    bool        hitBlock = false;
    BlockPos    pos;            // integer coordinates of the hit block
    std::string typeName;       // e.g. "minecraft:stone"
    std::string descriptionId;  // e.g. "tile.stone.stone"
    double      distance = 0.0; // distance from `from` to the hit point
};

// Cast a ray from `from` along the normalized `dir` through the block grid of
// `region`. Air blocks are transparent; when `passLiquids` is true water and
// lava are transparent too. Stops at the first opaque block, or at
// `maxDistance` blocks (then returns nullopt).
[[nodiscard]] std::optional<RaycastResult> raycastToBlock(
    IConstBlockSource const& region,
    Vec3 const&              from,
    Vec3 const&              dir,
    double                   maxDistance,
    bool                     passLiquids
);

// True when a block is one of the liquid blocks (water / lava) by type id.
[[nodiscard]] bool isLiquidBlock(Block const& block);

} // namespace insight
