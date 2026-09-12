#pragma once

#include <functional>
#include <string>

#include "Config.h"

#include "Raycast.h"

namespace insight {

// Everything needed to render one sample of the player's view.
struct LookInfo {
    bool hasTarget = false; // a block is under the crosshair (or showEmpty text is used)
    bool isEntity  = false; // target is an entity (block fields are empty)

    // block target fields
    std::string blockName; // localized display name (may fall back to the type id)
    std::string blockType; // e.g. "minecraft:stone"
    std::string blockKey;  // translation key, e.g. "tile.stone.stone"
    std::string extras;    // extra per-block-type lines ("\n"-joined, may be empty)
    std::string direction; // facing of the block for {direction} (may be empty)
    std::string light;     // light level 0..15 at the block for {light} (may be empty)
    std::string emission;  // light emitted by the block for {emission} (may be empty)

    // entity target fields
    std::string entityName; // player real name / name tag / localized type name
    std::string entityType; // e.g. "minecraft:zombie"
    int         health    = 0;
    int         maxHealth = 0;
    bool        hasHealth = false;

    int         x        = 0;
    int         y        = 0;
    int         z        = 0;
    double      distance = 0.0;
    std::string dimName; // overworld / nether / the_end
};

// Builds the LookInfo for one hit block. The display name is resolved with
// resolveDisplayName() for `langCode`; when no translation exists the raw
// translation key is shown (or the type id if there is no key at all).
[[nodiscard]] LookInfo makeBlockLookInfo(RaycastResult const& hit, std::string const& langCode);

// Builds the LookInfo for one hit entity.
[[nodiscard]] LookInfo
makeEntityLookInfo(std::string const& entityName, std::string const& entityType, int health, int maxHealth);

// Pick the format that applies to the current target (per-type overrides) or
// the global format / emptyText.
[[nodiscard]] std::string pickFormat(Config const& cfg, LookInfo const& info);

// Renders the final display string for `info` using `cfg` (placeholder
// substitution + color-code normalization).
[[nodiscard]] std::string renderText(Config const& cfg, LookInfo const& info);

} // namespace insight
