#include "Raycast.h"

#include <cmath>

#include "mc/world/level/block/Block.h"
#include "mc/world/level/material/Material.h"

namespace insight {

// Whether a block is a liquid is engine data (its material says so), which
// covers water, lava and their flowing variants without an id list.
bool isLiquidBlock(Block const& block) {
    try {
        return static_cast<bool>(block.getMaterial().mLiquid);
    } catch (...) {
        return false;
    }
}

namespace {

// floor to the block grid of a float component
constexpr int toInt(double v) { return static_cast<int>(std::floor(v)); }

} // namespace

std::optional<RaycastResult> raycastToBlock(
    IConstBlockSource const& region,
    Vec3 const&              from,
    Vec3 const&              dir,
    double                   maxDistance,
    bool                     passLiquids
) {
    // normalize
    double len = std::sqrt(
        static_cast<double>(dir.x) * dir.x + static_cast<double>(dir.y) * dir.y + static_cast<double>(dir.z) * dir.z
    );
    if (len < 1e-6) {
        return std::nullopt;
    }
    double stepX = static_cast<double>(dir.x) / len;
    double stepY = static_cast<double>(dir.y) / len;
    double stepZ = static_cast<double>(dir.z) / len;

    // march along the ray at a sub-block resolution (cheap, robust)
    constexpr double kStep = 0.2;
    double           maxT  = maxDistance > 0 ? maxDistance : 1.0;
    double           t     = kStep; // skip the voxel containing the eye position
    for (; t <= maxT + 1e-9; t += kStep) {
        double px = static_cast<double>(from.x) + stepX * t;
        double py = static_cast<double>(from.y) + stepY * t;
        double pz = static_cast<double>(from.z) + stepZ * t;

        BlockPos pos(toInt(px), toInt(py), toInt(pz));

        // If we left the loaded region, stop scanning (keeps the ray sane in
        // the void or at the world border).
        if (!region.hasChunksAt(pos, 0, false)) {
            return std::nullopt;
        }
        auto const& block = region.getBlock(pos);
        if (block.isAir()) {
            continue;
        }
        std::string typeName = block.getTypeName();
        if (passLiquids && isLiquidBlock(block)) {
            continue;
        }
        RaycastResult res;
        res.hitBlock      = true;
        res.pos           = pos;
        res.typeName      = std::move(typeName);
        res.descriptionId = block.getDescriptionId();
        res.distance      = t;
        return res;
    }
    return std::nullopt;
}

} // namespace insight
