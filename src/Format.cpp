#include "Format.h"

#include <map>
#include <string_view>

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

std::string pickFormat(Config const& cfg, LookInfo const& info) {
    if (!info.hasTarget) {
        return cfg.emptyText;
    }
    if (info.isEntity) {
        return cfg.entityFormat;
    }
    for (auto const& ov : cfg.overrides) {
        if (!ov.match.empty()) {
            auto haystack = util::toLower(info.blockType);
            auto needle   = util::toLower(ov.match);
            if (haystack.find(needle) != std::string::npos) {
                return ov.format;
            }
        }
    }
    return cfg.format;
}

std::string renderText(Config const& cfg, LookInfo const& info) {
    std::string text = pickFormat(cfg, info);

    // placeholder substitution (values may be empty -> the placeholder is
    // removed so it never shows up as raw text)
    auto setVal = [&text](std::string_view ph, std::string const& value) {
        util::replaceAll(text, std::string(ph), value);
    };

    if (!info.hasTarget) {
        // nothing targeted: every placeholder vanishes
        setVal("{blockType}", "");
        setVal("{blockName}", "");
        setVal("{blockKey}", "");
        setVal("{x}", "");
        setVal("{y}", "");
        setVal("{z}", "");
        setVal("{dist}", "");
        setVal("{direction}", "");
        setVal("{light}", "");
        setVal("{emission}", "");
        setVal("{entityName}", "");
        setVal("{entityType}", "");
        setVal("{health}", "");
        setVal("{maxHealth}", "");
    } else if (info.isEntity) {
        setVal("{entityName}", info.entityName);
        setVal("{entityType}", info.entityType);
        std::string hpText = info.hasHealth ? std::to_string(info.health) : "";
        setVal("{health}", hpText);
        std::string maxHpText = info.hasHealth ? std::to_string(info.maxHealth) : "";
        setVal("{maxHealth}", maxHpText);
        // block placeholders are not used in entity view
        setVal("{blockType}", "");
        setVal("{blockName}", "");
        setVal("{blockKey}", "");
        setVal("{x}", "");
        setVal("{y}", "");
        setVal("{z}", "");
        setVal("{direction}", "");
        setVal("{light}", "");
        setVal("{emission}", "");
    } else {
        setVal("{blockType}", info.blockType);
        setVal("{blockName}", info.blockName);
        setVal("{blockKey}", info.blockKey);
        setVal("{x}", std::to_string(info.x));
        setVal("{y}", std::to_string(info.y));
        setVal("{z}", std::to_string(info.z));
        setVal("{dist}", util::trimNumber(info.distance, 1));
        setVal("{direction}", info.direction);
        setVal("{light}", info.light);
        setVal("{emission}", info.emission);
        setVal("{entityName}", "");
        setVal("{entityType}", "");
        setVal("{health}", "");
        setVal("{maxHealth}", "");
    }
    setVal("{dim}", info.dimName);
    setVal("{extras}", info.extras);

    // trailing whitespace-only lines are removed so popups don't get blank gaps
    auto lines = util::splitLines(text);
    while (!lines.empty() && util::trimInPlace(lines.back()).empty()) {
        lines.pop_back();
    }
    text.clear();
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i) {
            text += '\n';
        }
        text += lines[i];
    }

    return util::colorizeAmpersand(text);
}

} // namespace insight
