#include "I18n.h"

#include <vector>

#include "ll/api/i18n/I18n.h"

namespace insight {

std::string trRaw(std::string_view localeCode, std::string_view key) {
    auto& i18n = ll::i18n::getInstance();

    // candidate locales, most specific first
    std::vector<std::string_view> candidates;
    if (localeCode.empty()) {
        candidates.emplace_back(ll::i18n::getDefaultLocaleCode());
    } else {
        candidates.emplace_back(localeCode);
        if (auto separator = localeCode.find_first_of("-_"); separator != std::string_view::npos) {
            candidates.emplace_back(localeCode.substr(0, separator));
        }
    }
    // only the simplified file ships: serve it to every Chinese variant
    if (!candidates.empty() && candidates.back().rfind("zh", 0) == 0) {
        candidates.emplace_back("zh_cn");
    }
    candidates.emplace_back("en");

    for (auto candidate : candidates) {
        if (candidate.empty()) {
            continue;
        }
        if (auto found = i18n.get(key, candidate); !found.empty()) {
            return std::string(found);
        }
    }

    // unknown key (or no language file at all): show the key, never nothing
    return std::string(key);
}

} // namespace insight
