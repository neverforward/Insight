#pragma once

#include <string>
#include <string_view>

#include <fmt/format.h>

namespace insight {

// ---------------------------------------------------------------------------
// Message lookup on top of LeviLamina's i18n (ll/api/i18n/I18n.h).
//
// The language files themselves live in `lang/<locale>.json` next to the mod
// and are loaded once by Insight::load() through
// `ll::i18n::getInstance().load(getSelf().getLangDir())` - exactly the layout
// the LeviLamina i18n guide describes.
//
// This wrapper only adds the fallback chain the raw API does not do, so a
// player whose locale has no file never sees an empty line:
//
//   LeviLamina default locale (when `localeCode` is empty)
//     -> exact locale ("zh_cn") -> base language ("zh")
//     -> "zh_cn" for any other Chinese variant -> "en" -> the key itself
// ---------------------------------------------------------------------------

/// Looks `key` up for `localeCode`. Returns the key itself when nothing
/// matches, so callers never render an empty string by accident.
[[nodiscard]] std::string trRaw(std::string_view localeCode, std::string_view key);

/// Same as trRaw(), with fmt-style substitution of `args` ({0}, {1}, ...).
/// A malformed pattern falls back to the unformatted text.
template <class... Args>
[[nodiscard]] std::string tr(std::string_view localeCode, std::string_view key, Args&&... args) {
    std::string pattern = trRaw(localeCode, key);
    if constexpr (sizeof...(Args) == 0) {
        return pattern;
    } else {
        try {
            return fmt::vformat(pattern, fmt::make_format_args(args...));
        } catch (fmt::format_error const&) {
            return pattern;
        }
    }
}

} // namespace insight
