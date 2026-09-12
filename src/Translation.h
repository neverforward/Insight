#pragma once

#include <string>

namespace insight {

// Translates a *vanilla* localization key through the engine's own localization
// tables: `I18n::getLocaleFor(code)` returns the Localization that the engine
// built for that language, which already merges the vanilla pack with every
// other resource pack the player has enabled - so resource pack translations are
// honoured without reading any file ourselves.
//
// Returns "" when the engine has no translation for the key; the caller decides
// what to show then (usually the raw key).
[[nodiscard]] std::string translateLocalizationKey(std::string const& key, std::string const& langCode);

// Resolves a display name for a vanilla key. The engine's localization is asked
// first, in this order:
//   1) the requested language (`langCode`, e.g. the player's own locale),
//   2) the language the engine currently runs with,
//   3) "en_US",
// and for each of them the key itself and the "<key>.name" display form - and,
// for "tile.<name>" keys, the item form "item.<name>" (Block::getDescriptionId()
// returns the legacy "tile.<name>" while some tables only carry the item key).
// Returns the raw key itself when nothing translates.
[[nodiscard]] std::string resolveDisplayName(std::string const& key, std::string const& langCode);

} // namespace insight
