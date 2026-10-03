#include "Format.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "Colors.h"
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
    // The float pair is what the icons and the bar fill against; the client replaces it
    // with the attribute's exact value, the server keeps the integer reading.
    info.healthExact = static_cast<float>(health);
    info.healthMax   = static_cast<float>(maxHealth);
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

// Bar pieces: one block per step. The filled part of the bar is drawn in the colour the
// config gives that stat (red for the hit points by default), the empty rest in the muted
// label colour, so the track reads as a background rather than as a second bar.
constexpr char kBarFull[] = "\xE2\x96\x88"; // U+2588
constexpr char kBarHalf[] = "\xE2\x96\x8C"; // U+258C

// Armor has no attribute of its own: the vanilla bar (and the client's armor icons) top
// out at twenty points, which is what the text bar fills against too.
constexpr float kArmorFull = 20.0f;

// The bar as text: `steps` blocks wide, filled in proportion to the value, the rest a
// track in another colour. This is what a server-side channel sends - the client paints
// its own bar instead, but the two are the same fixed-width shape.
std::string barRun(std::string const& fillColor, std::string const& trackColor, float points, float full, int steps) {
    steps                = std::max(1, steps);
    float const capacity = full > 0.0f ? full : 1.0f;
    float const progress = std::clamp(points, 0.0f, capacity) / capacity * static_cast<float>(steps);
    int const   whole    = std::min(steps, static_cast<int>(std::floor(progress)));
    bool const  half     = whole < steps && progress - static_cast<float>(whole) >= 0.5f;

    std::string fill;
    for (int i = 0; i < whole; ++i) {
        fill += kBarFull;
    }
    if (half) {
        fill += kBarHalf;
    }
    std::string track;
    for (int i = whole + (half ? 1 : 0); i < steps; ++i) {
        track += kBarFull;
    }
    return colored(fillColor, fill) + colored(trackColor, track);
}

// One "label value" part, the shape the per-block extras use: the label in the
// panel's muted colour, the value in the default colour. Empty for a value this
// target does not have, so a switch that is off - or a value we could not read -
// drops its part instead of leaving a bare label behind. Both colours come from
// colors.label / colors.value.
std::string labelled(
    std::string const&  localeCode,
    char const*         labelKey,
    std::string const&  value,
    ColorOptions const& colors
) {
    if (value.empty()) {
        return {};
    }
    return colored(colors.label, tr(localeCode, labelKey)) + " " + colored(colors.value, value);
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
// "Stone§7 (north)". The brackets are only there when there is a facing to show.
std::string nameLine(
    std::string const&  name,
    std::string const&  facing,
    bool                showName,
    bool                showFacing,
    ColorOptions const& colors
) {
    std::string line = showName ? colored(colors.name, name) : std::string{};
    if (showFacing && !facing.empty()) {
        if (!line.empty()) {
            line += ' ';
        }
        line += colored(colors.facing, "(" + facing + ")");
    }
    return line;
}

// Line 2: the type id, with the key its name was resolved from in brackets right
// after it, e.g. "§8minecraft:stone §8(tile.stone.stone)".
std::string idLine(
    std::string const&  id,
    std::string const&  key,
    bool                showId,
    bool                showKey,
    ColorOptions const& colors
) {
    std::string line;
    if (showId && !id.empty()) {
        line = colored(colors.identifier, id);
    }
    if (showKey && !key.empty()) {
        if (!line.empty()) {
            line += ' ';
        }
        line += colored(colors.translationKey, "(" + key + ")");
    }
    return line;
}

// Line 3: x, y, z and the distance, each in its own configured colour.
std::string positionLine(LookInfo const& info, bool showPosition, bool showDistance, ColorOptions const& colors) {
    std::string line;
    if (showPosition) {
        line = colored(colors.x, std::to_string(info.x)) + ", " + colored(colors.y, std::to_string(info.y)) + ", "
             + colored(colors.z, std::to_string(info.z));
    }
    if (showDistance) {
        auto const distance = util::trimNumber(info.distance, 1);
        if (!distance.empty()) {
            if (!line.empty()) {
                line += ' ';
            }
            line += colored(colors.distance, "[" + distance + "]");
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
    auto const&              c = cfg.colors;
    std::vector<std::string> lines;

    pushLine(lines, nameLine(info.blockName, info.direction, d.name, d.facing, c));
    pushLine(lines, idLine(info.blockType, info.blockKey, d.identifier, d.translationKey, c));
    pushLine(lines, positionLine(info, d.position, d.distance, c));
    pushLine(
        lines,
        joined({
            d.light ? labelled(localeCode, "Light", info.light, c) : std::string{},
            d.emission ? labelled(localeCode, "Emission", info.emission, c) : std::string{},
        })
    );
    appendExtras(lines, info.extras, d.extras);
    return joinLines(lines);
}

// The entity panel: name (compass facing), type id (key), position (distance), hit
// points, then the per-entity extras. No light level and no light emission: those
// describe a position in the world, not the entity under the crosshair.
std::string renderEntityText(Config const& cfg, LookInfo& info, std::string const& localeCode) {
    auto const&              e = cfg.entity;
    auto const&              c = cfg.colors;
    std::vector<std::string> lines;

    pushLine(lines, nameLine(info.entityName, info.direction, e.name, e.facing, c));
    pushLine(lines, idLine(info.entityType, info.entityKey, e.identifier, e.translationKey, c));
    pushLine(lines, positionLine(info, e.position, e.distance, c));

    // Hit points and armor. The client paints a sprite row or a bar over these two lines;
    // what the text says is what a server-side channel sends, and there the icon rows are
    // deliberately *not* spelled out in font glyphs (they read badly in a vanilla channel)
    // - only the bar is, as a run of blocks. The number form is always what the rows carry
    // otherwise, and the client shows the same "label + number" behind its sprite row in
    // the "+number" styles.
    std::string const style    = e.healthStyle;
    bool const        barStyle = style == "bar" || style == "bar+number";
    int const         steps    = std::max(1, e.heartsPerRow);

    std::string health;
    if (e.health && info.hasHealth) {
        std::string const numbers =
            e.healthDecimals && info.healthMax > 0.0f
                ? util::trimNumber(info.healthExact, 1) + "/" + util::trimNumber(info.healthMax, 0)
                : healthText(info.health, info.maxHealth);

        // What sits next to a sprite row: the label and the plain numbers.
        info.healthText = colored(c.label, tr(localeCode, "Health")) + " " + colored(c.health, numbers);

        std::string value = colored(c.health, numbers);
        if (barStyle && info.healthMax > 0.0f) {
            value = barRun(c.health, c.label, info.healthExact, info.healthMax, steps);
            if (style == "bar+number") {
                value += " " + colored(c.health, numbers);
            }
        }
        health = colored(c.label, tr(localeCode, "Health")) + " " + value;
    }
    if (!health.empty()) {
        info.healthLine = static_cast<int>(lines.size());
        lines.push_back(std::move(health));
    }

    // Armor, right under the hit points, and only when the subject has some: a mob
    // without armor shows nothing rather than "0", the way every other part of the
    // panel drops what the target does not have.
    if (e.armor && info.hasArmor) {
        std::string const numbers = util::trimNumber(info.armorValue, 0);
        info.armorText            = colored(c.label, tr(localeCode, "Armor")) + " " + colored(c.value, numbers);

        std::string value = colored(c.value, numbers);
        if (barStyle) {
            value = barRun(c.value, c.label, info.armorValue, kArmorFull, steps);
            if (style == "bar+number") {
                value += " " + colored(c.value, numbers);
            }
        }
        info.armorLine = static_cast<int>(lines.size());
        lines.push_back(colored(c.label, tr(localeCode, "Armor")) + " " + value);
    }

    appendExtras(lines, info.extras, e.extras);
    return joinLines(lines);
}

std::string renderEmptyText(Config const& cfg) { return cfg.emptyText; }

} // namespace insight
