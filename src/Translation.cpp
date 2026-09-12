#include "Translation.h"

#include <cstring>
#include <memory>

#include "mc/locale/I18n.h"
#include "mc/locale/Localization.h"

namespace insight {

namespace {

// One lookup in one language table: the key itself, then the "<key>.name"
// display form the vanilla tables use (e.g. "tile.chest.name").
std::string lookupIn(std::shared_ptr<Localization const> const& locale, std::string const& key) {
    if (!locale) {
        return {};
    }
    try {
        std::string out;
        if (locale->get(key, out, {})) {
            return out;
        }
        std::string display;
        if (locale->get(key + ".name", display, {})) {
            return display;
        }
    } catch (...) {}
    return {};
}

} // namespace

std::string translateLocalizationKey(std::string const& key, std::string const& langCode) {
    if (key.empty()) {
        return {};
    }
    try {
        auto& i18n = getI18n(); // mc/locale/I18n.h global accessor

        if (!langCode.empty()) {
            if (auto text = lookupIn(i18n.getLocaleFor(langCode), key); !text.empty()) {
                return text;
            }
        }
        // the language the engine itself runs with (e.g. the client's UI
        // language, which also covers resource packs that add new languages)
        if (auto text = lookupIn(i18n.getCurrentLanguage(), key); !text.empty()) {
            return text;
        }
        // last resort: English, then give up
        return lookupIn(i18n.getLocaleFor("en_US"), key);
    } catch (...) {
        return {};
    }
}

std::string resolveDisplayName(std::string const& key, std::string const& langCode) {
    if (key.empty()) {
        return {};
    }

    // Build the candidate keys. Block::getDescriptionId() returns the legacy
    // "tile.<name>" form, but some tables only carry the item form
    // "item.<name>", so both are tried.
    std::string cands[2] = {key, key};
    if (key.rfind("tile.", 0) == 0) {
        cands[1] = "item." + key.substr(std::strlen("tile."));
    }
    if (cands[1] == cands[0]) {
        cands[1].clear();
    }

    for (auto const& cand : cands) {
        if (cand.empty()) {
            continue;
        }
        if (auto translated = translateLocalizationKey(cand, langCode); !translated.empty()) {
            return translated;
        }
    }

    // nothing translated: show the raw key
    return key;
}

} // namespace insight
