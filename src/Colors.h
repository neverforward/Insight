#pragma once

#include <optional>
#include <string>

namespace insight {

// ---------------------------------------------------------------------------
// Colour codes of the panel.
//
// The panel is plain text with vanilla formatting codes in it, so every colour is
// one legacy code. The configuration stores just that one letter - without the
// "§" - because that is the character a player types, and it keeps the config file
// readable. An empty code means "no code at all", and the piece of text then keeps
// whatever colour the renderer is already using (the client's client.textColor).
//
// Bedrock has more of them than Java: besides the sixteen 0-9/a-f codes it also
// understands g (minecoin gold), h (quartz), i (iron), j (netherite), m
// (redstone), n (copper), p (gold), q (emerald), s (diamond), t (lapis), u
// (amethyst), v (resin) and w (party blue). k, l, o and r are *formatting* codes
// (obfuscated, bold, italic, reset), not colours, and are deliberately rejected:
// one of those on a line would change how the rest of the panel is drawn.
// ---------------------------------------------------------------------------

// The "§" that starts a formatting code, as UTF-8.
inline constexpr char kSectionSign[] = "\xC2\xA7";

// True for a stored code: empty, or exactly one character this mod accepts as a
// colour.
[[nodiscard]] bool isColorCode(std::string const& code);

// Turns what a player typed into the code this mod stores. Accepts the bare
// letter ("c"), the "§" form ("§c") and the ampersand form ("&c"), in either
// case; "#c" is accepted too, for people who read the config's hex colours as a
// template. An empty input (or "none"/"off"/"default") clears the colour, which
// is why this returns a string rather than an optional. nullopt means the value
// is not a colour at all.
[[nodiscard]] std::optional<std::string> normalizeColorCode(std::string const& value);

// "§c" for a valid code, "" for anything else.
[[nodiscard]] std::string colorPrefix(std::string const& code);

// `text` in the configured colour; an empty or invalid code returns it
// unchanged, so a target without a configured colour never gets a stray code.
[[nodiscard]] std::string colored(std::string const& code, std::string const& text);

// The colour codes this mod offers, in the order the configuration screen lists
// them (the sixteen everyone knows, then the Bedrock extras).
[[nodiscard]] std::string const& colorCodeChoices();

// The RGB a code draws in (0..1 per channel). Needed where the panel paints with
// a colour itself instead of handing a "§" code to the text renderer - the hit
// point and armor bars take their fill from colors.health / colors.value.
// Returns false for an empty or unknown code, leaving the caller's colour alone.
[[nodiscard]] bool colorRgb(std::string const& code, float& r, float& g, float& b);

} // namespace insight
