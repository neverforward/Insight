#include "Format.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#include "I18n.h"
#include "Translation.h"
#include "Util.h"

namespace insight {

LookInfo makeBlockLookInfo(RaycastResult const& hit, std::string const& langCode) {
    LookInfo info;
    info.hasTarget = true;
    info.blockType = hit.typeName;
    info.blockKey  = hit.descriptionId;
    info.x         = hit.pos.x;
    info.y         = hit.pos.y;
    info.z         = hit.pos.z;
    info.distance  = hit.distance;

    // name resolution chain: engine i18n -> "<key>.name" -> vanilla .lang
    // file for `langCode` -> raw key (see resolveDisplayName)
    std::string name = resolveDisplayName(hit.descriptionId, langCode);
    info.blockName   = name.empty() ? (hit.descriptionId.empty() ? hit.typeName : hit.descriptionId) : name;
    return info;
}

LookInfo makeEntityLookInfo(std::string const& entityName, std::string const& entityType, int health, int maxHealth) {
    LookInfo info;
    info.hasTarget  = true;
    info.isEntity   = true;
    info.entityName = entityName;
    info.entityType = entityType;
    if (maxHealth > 0) {
        info.hasHealth = true;
        info.health    = health;
        info.maxHealth = maxHealth;
    }
    return info;
}

namespace {
// The hit points of an entity as "current/max". Kept as its own function because a
// bar in front of / behind the numbers is being considered again later.
std::string healthText(int health, int maxHealth) {
    if (maxHealth <= 0) {
        return {};
    }
    return std::to_string(health) + "/" + std::to_string(maxHealth);
}

// One "label value" part, the shape the per-block extras use: the label in the
// panel's muted colour, the value in the default colour. Empty for a value this
// target does not have, so a switch that is off - or a value we could not read -
// drops its part instead of leaving a bare label behind.
std::string labelled(std::string const& localeCode, char const* labelKey, std::string const& value) {
    if (value.empty()) {
        return {};
    }
    return std::string("§7") + tr(localeCode, labelKey) + " §f" + value;
}

// Joins the parts that survived with a single space, so a line with one part left
// carries no stray separator.
std::string joined(std::vector<std::string> const& parts) {
    std::string line;
    for (auto const& part : parts) {
        if (part.empty()) {
            continue;
        }
        if (!line.empty()) {
            line += ' ';
        }
        line += part;
    }
    return line;
}

// A line is only worth drawing when it has content: an empty line would show up as
// a blank gap in the panel.
void pushLine(std::vector<std::string>& lines, std::string line) {
    if (!line.empty()) {
        lines.push_back(std::move(line));
    }
}

// Line 1: the name, with the facing hanging off it in the muted colour, e.g.
// "Stone§7(north)". The brackets are only there when there is a facing to show.
std::string nameLine(std::string const& name, std::string const& facing, bool showName, bool showFacing) {
    std::string line = showName ? name : std::string{};
    if (showFacing && !facing.empty()) {
        line += "§7 (" + facing + ")";
    }
    return line;
}

// Line 2: the type id, with the key its name was resolved from in brackets right
// after it, both muted, e.g. "§7minecraft:stone(tile.stone.stone)".
std::string idLine(std::string const& id, std::string const& key, bool showId, bool showKey) {
    std::string line;
    if (showId && !id.empty()) {
        line = "§7" + id;
    }
    if (showKey && !key.empty()) {
        if (line.empty()) {
            line = "§7";
        }
        line += " (" + key + ")";
    }
    return line;
}

// Line 3: x, y, z in the default colour, the distance muted behind it.
std::string positionLine(LookInfo const& info, bool showPosition, bool showDistance) {
    std::string line;
    if (showPosition) {
        line = std::to_string(info.x) + ", " + std::to_string(info.y) + ", " + std::to_string(info.z);
    }
    if (showDistance) {
        auto const distance = util::trimNumber(info.distance, 1);
        if (!distance.empty()) {
            if (!line.empty()) {
                line += ' ';
            }
            line += " §7[" + distance + "]";
        }
    }
    return line;
}

// The extra per-type lines, last.
void appendExtras(std::vector<std::string>& lines, std::string const& extras, bool show) {
    if (!show) {
        return;
    }
    for (auto& extra : util::splitLines(extras)) {
        pushLine(lines, std::move(extra));
    }
}

std::string joinLines(std::vector<std::string> const& lines) {
    std::string text;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i) {
            text += '\n';
        }
        text += lines[i];
    }
    return text;
}

} // namespace

// The block panel: name (facing), type id (key), position (distance), light level
// and light emission, then the per-block-type extras.
std::string renderBlockText(Config const& cfg, LookInfo const& info, std::string const& localeCode) {
    auto const&              d = cfg.display;
    std::vector<std::string> lines;

    pushLine(lines, nameLine(info.blockName, info.direction, d.name, d.facing));
    pushLine(lines, idLine(info.blockType, info.blockKey, d.identifier, d.translationKey));
    pushLine(lines, positionLine(info, d.position, d.distance));
    pushLine(
        lines,
        joined({
            d.light ? labelled(localeCode, "Light", info.light) : std::string{},
            d.emission ? labelled(localeCode, "Emission", info.emission) : std::string{},
        })
    );
    appendExtras(lines, info.extras, d.extras);
    return joinLines(lines);
}

// The entity panel: name (compass facing), type id (key), position (distance), hit
// points, then the per-entity extras. No light level and no light emission: those
// describe a position in the world, not the entity under the crosshair.
std::string renderEntityText(Config const& cfg, LookInfo const& info, std::string const& localeCode) {
    auto const&              e = cfg.entity;
    std::vector<std::string> lines;

    pushLine(lines, nameLine(info.entityName, info.direction, e.name, e.facing));
    pushLine(lines, idLine(info.entityType, info.entityKey, e.identifier, e.translationKey));
    pushLine(lines, positionLine(info, e.position, e.distance));

    std::string health;
    if (e.health && info.hasHealth) {
        health = healthText(info.health, info.maxHealth);
    }
    pushLine(lines, labelled(localeCode, "Health", health));

    appendExtras(lines, info.extras, e.extras);
    return joinLines(lines);
}

std::string renderEmptyText(Config const& cfg) { return cfg.emptyText; }

} // namespace insight
