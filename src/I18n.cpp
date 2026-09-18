#include "I18n.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#endif

#include "ll/api/i18n/I18n.h"

#include "Insight.h"
#include "Util.h"

namespace insight {

namespace {

/// Exact file stems of the message files this mod ships ("en", "zh_cn", ...).
std::vector<std::string> const& localeStems() {
    static std::vector<std::string> const stems = [] {
        std::vector<std::string> out;
        std::error_code          error;
        auto const               dir = Insight::getInstance().getSelf().getLangDir();
        for (auto const& entry : std::filesystem::directory_iterator(dir, error)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                out.push_back(entry.path().stem().string());
            }
        }
        if (out.empty()) {
            out.emplace_back("en"); // nothing shipped: keys are shown as they are
        }
        return out;
    }();
    return stems;
}

/// Exact stem for a locale code, matched case-insensitively ("zh_CN" -> "zh_cn"),
/// with the base language as a second chance. Returns "" when nothing matches.
///
/// This step is essential: ll::i18n silently falls back to the default locale for
/// an unknown code and still returns a *non-empty* string, so querying it with
/// "zh_CN" (the code the client reports) hands back the English text and every
/// later candidate is never tried.
std::string canonicalLocale(std::string_view code) {
    if (code.empty()) {
        return {};
    }
    auto const lowered = util::toLower(std::string{code});
    for (auto const& stem : localeStems()) {
        if (stem == lowered) {
            return stem;
        }
    }
    if (auto separator = lowered.find_first_of("-_"); separator != std::string::npos) {
        auto const base = lowered.substr(0, separator);
        for (auto const& stem : localeStems()) {
            if (stem == base) {
                return stem;
            }
        }
    }
    return {};
}

/// Operating-system UI language, used only as a hint for "auto".
std::string systemLanguageCode() {
#ifdef _WIN32
    switch (PRIMARYLANGID(::GetUserDefaultUILanguage())) {
    case LANG_CHINESE:
        return "zh_cn";
    case LANG_JAPANESE:
        return "ja_jp";
    case LANG_KOREAN:
        return "ko_kr";
    case LANG_GERMAN:
        return "de_de";
    case LANG_FRENCH:
        return "fr_fr";
    case LANG_SPANISH:
        return "es_es";
    case LANG_RUSSIAN:
        return "ru_ru";
    default:
        return "en";
    }
#else
    return "en";
#endif
}

} // namespace

std::string trRaw(std::string_view localeCode, std::string_view key) {
    auto& i18n = ll::i18n::getInstance();

    // candidate locales, most specific first; every one of them is an existing
    // message file, so a lookup can never succeed by falling back to English
    std::vector<std::string> candidates;
    if (auto exact = canonicalLocale(localeCode); !exact.empty()) {
        candidates.push_back(exact);
    }
    // only the simplified file ships: serve it to every Chinese variant
    if (!candidates.empty() && candidates.front().rfind("zh", 0) == 0) {
        if (auto simplified = canonicalLocale("zh_cn"); !simplified.empty()) {
            candidates.push_back(simplified);
        }
    }
    if (auto english = canonicalLocale("en"); !english.empty()) {
        candidates.push_back(english);
    }
    if (candidates.empty()) {
        candidates = localeStems();
    }

    for (auto const& candidate : candidates) {
        if (auto found = i18n.get(key, candidate); !found.empty()) {
            return std::string(found);
        }
    }

    // unknown key (or no language file at all): show the key, never nothing
    return std::string(key);
}

std::string resolveLanguageCode(std::string const& configured, std::string const& engineLanguage) {
    // An explicit choice always wins. This follows LHolo, which has no "follow the
    // game" mode at all: the client's reported language is unreliable here (this
    // GDK client answers "en_US" even on a Chinese installation), so the language
    // is a stored preference instead of something detected at every lookup.
    if (!configured.empty() && configured != "auto") {
        if (auto picked = canonicalLocale(configured); !picked.empty()) {
            return picked;
        }
        return "zh_cn";
    }

    // "auto": only a language we actually ship counts as a signal, and a bare
    // English answer is ignored because that is what this client reports when it
    // has nothing to say.
    if (auto engine = canonicalLocale(engineLanguage); !engine.empty() && engine != "en") {
        return engine;
    }
    if (auto system = canonicalLocale(systemLanguageCode()); !system.empty() && system != "en") {
        return system;
    }
    return "zh_cn";
}

} // namespace insight
