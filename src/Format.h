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

    // entity target fields
    std::string entityName; // player real name / name tag / localized type name
    std::string entityType; // e.g. "minecraft:zombie"
    std::string entityKey;  // the key the name is resolved from, e.g. "entity.zombie"

    // fields both targets use
    std::string extras;    // extra per-type lines ("\n"-joined, may be empty)
    std::string direction; // facing for the name line: a block's facing state, or an
                           // entity's own yaw as a compass point (may be empty)
    std::string light;     // light level 0..15 at the block (may be empty; blocks only)
    std::string emission;  // light the block itself emits (may be empty; blocks only)

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

// Renders the panel text for one sample. Blocks and entities have separate layouts
// and separate switches (DisplayOptions / EntityOptions in Config.h), so each has
// its own renderer; both return an empty string when nothing is switched on. Labels
// and compass words are translated for `localeCode` (the player's locale on the
// server, the configured language on the client).
[[nodiscard]] std::string renderBlockText(Config const& cfg, LookInfo const& info, std::string const& localeCode);
[[nodiscard]] std::string renderEntityText(Config const& cfg, LookInfo const& info, std::string const& localeCode);

// The text shown when nothing is targeted (showEmpty is on): the configured empty
// text with its & colour codes resolved. There are no placeholders any more.
[[nodiscard]] std::string renderEmptyText(Config const& cfg);

} // namespace insight
