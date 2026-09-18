#pragma once

#include <string>
#include <vector>

namespace insight {

// ---------------------------------------------------------------------------
// Insight - a Jade-like "what am I looking at" display mod for Minecraft
// Bedrock (LeviLamina). This struct is the whole configuration. It is
// (de)serialized to `plugins/Insight/config/config.json` (server) or
// `mods/Insight/config/config.json` (client) through ll::config, which uses
// aggregate reflection: keep this file a plain aggregate of primitives,
// std::string / std::vector / nested plain aggregates.
// ---------------------------------------------------------------------------

// One optional per-block-type display override. `match` is matched against
// the block type id (e.g. "minecraft:chest") as a case-insensitive substring.
struct BlockOverride {
    std::string match  = "";
    std::string format = "";
};

// Server (BDS) only options.
struct ServerOptions {
    // Display channel used by the server build:
    //   "actionbar" (default) | "tip" | "popup" | "jukebox" | "system" | "chat" | "none"
    std::string channel = "actionbar";
};

// Client (GDK / LeviLamina client) only options.
struct ClientOptions {
    // Draw the on-screen overlay at all.
    bool showOverlay = true;

    // Anchor of the overlay on screen:
    //   "top_left" "top_center" "top_right"
    //   "middle_left" "center" "middle_right"
    //   "bottom_left" "bottom_center" "bottom_right"
    std::string anchor = "bottom_center";

    // Extra offset for the anchor, as a fraction of the screen size. A positive
    // value always moves the panel away from the edge it is anchored at, towards
    // the middle of the screen (for centre anchors: right/down).
    // (0.05 == 5% of screen width / height). Positive x moves right,
    // positive y moves *up* (screen space is y-up in the Bedrock UI).
    float offsetX = 0.0f;
    float offsetY = 0.0f;

    // Font size multiplier (1.0 = default UI font scale).
    float fontSize = 1.0f;

    // Dark panel behind the text (true) or plain text (false).
    bool background = true;

    // Panel opacity 0..1 when background is enabled.
    float backgroundAlpha = 0.45f;

    // Draw a text shadow.
    bool shadow = true;

    // Text color as RRGGBB hex when the text does not contain its own color
    // code. Ignored for the empty/default channel.
    std::string textColor = "ffffff";

    // Max overlay width as a fraction of screen width (text wraps). 0 = no wrap.
    float maxWidth = 0.0f;

    // Language of the interface and of the localized block names on the *client*:
    // "zh_cn", "en", or "auto". It is a stored preference rather than something
    // detected at runtime - this client answers "en_US" even on a Chinese
    // installation, so "auto" only follows it when it reports a language this mod
    // actually ships.
    std::string language = "zh_cn";

    // Overlay behaviour while connected to a remote (multiplayer) server:
    //   "off" (default): auto-hide the local overlay when playing online -
    //         container contents / furnace data / entity stats are
    //         server-authoritative and cannot be read from the client anyway;
    //         the server-side Insight pushes the info through its channel.
    //   "on": always draw the overlay; online only data that the client
    //         itself can see is shown.
    std::string overlayOnRemote = "off";

    // Hide the overlay while a game UI screen (inventory, chest, pause, ...)
    // is open, like Jade does.
    bool hideOverlayInGui = true;

    // --- key bindings (client only) ------------------------------------
    // Windows virtual-key codes (e.g. 0x49 = I, 0x4B = K); 0 disables the
    // binding. Both bindings are registered with LeviLamina, so they also show
    // up in the game's own key settings and can be remapped there.
    int keyOpenConfig = 0x49; // open the configuration screen
    int keyToggleShow = 0x4B; // show/hide the info display
};

// Per-block-type extra info adapters (everything can be switched off
// individually). Data availability differs by platform: containers and block
// entities exist server-side and in a local (single player) client world; on
// a client connected to a remote server only local world data is visible, so
// some adapters may silently show nothing there.
struct BlockExtrasConfig {
    bool enabled  = true; // master switch for all adapters below
    bool chest    = true; // container occupancy (chest/barrel/hopper/…): filled slots/total
    bool furnace  = true; // furnace / blast furnace / smoker slot usage
    bool brewing  = true; // brewing stand slot usage
    bool redstone = true; // comparator output signal
    bool misc     = true; // jukebox record + flower pot plant
};

struct Config {
    // Config schema version. IMPORTANT: bump this whenever a field is added,
    // removed or renamed. LeviLamina only merges the current defaults into an
    // existing file when this number differs from the file's "version", so a
    // new field without a bump makes deserialization fail ("missing required
    // field") and the mod refuses to load. 1 -> 2: added the client key
    // bindings (keyOpenConfig / keyToggleShow).
    int version = 2;

    // Master switch for the whole mod.
    bool enabled = true;

    // Default value for the per-player switch (a player may toggle it
    // individually with `/insight`).
    bool enabledByDefault = true;

    // How far (in blocks) the look ray travels.
    float maxDistance = 16.0f;

    // How often a player's view is sampled, in ticks (20 ticks = 1 s).
    int intervalTicks = 4;

    // Treat water/lava as transparent for targeting (look "through" liquids).
    bool passThroughLiquids = true;

    // Show something even when the player is looking at air / beyond range.
    bool showEmpty = false;

    // Text shown when showEmpty is true and nothing is targeted.
    std::string emptyText = "";

    // Format of the info, supports \n and the placeholders below:
    //   {blockType}  block type id             e.g. "minecraft:stone"
    //   {blockName}  localized block name      e.g. "石头" / "Stone"
    //   {blockKey}   translation key           e.g. "tile.stone.stone"
    //   {x} {y} {z}  integer position of the block
    //   {dist}       distance to the block in blocks (e.g. 3.5)
    //   {dim}        dimension name (overworld / nether / the_end)
    //   {direction}  facing of the block ("北"/"north", "上"/"up"; empty when
    //                the block has no facing state we understand)
    //   {light}      light level at the block (0..15; empty when unavailable)
    //   {emission}   light emitted by the block itself (0..15), e.g. 15 for
    //                glowstone, 0 for stone
    //   {extras}     per-block-type extra lines ("\n"-joined; empty when the
    //                targeted block has no extra info or extras are disabled)
    // Use & and the vanilla color codes (&0-9a-f, &l &o &n &m &r) or §-codes;
    // & is converted to § automatically. && produces a literal &.
    std::string format = "{blockName}\n§7{blockType} §8· §7{x}, {y}, {z}\n{extras}";

    // Show entities under the crosshair (when an entity is closer than any
    // hit block, or no block is hit).
    bool entityEnabled = true;

    // Format used while looking at an entity:
    //   {entityName}  display name (player real name / name tag / localized type)
    //   {entityType}  entity type id (e.g. "minecraft:zombie")
    //   {health} {maxHealth}  hit points (hidden when the target has none)
    //   {dist} {dim} also work.
    // Kept language-neutral on purpose: a default containing English words
    // would show up untranslated for every other locale.
    std::string entityFormat = "{entityName}\n§7{entityType} §8· §7{health}/{maxHealth}";

    // Optional per-block-type format overrides (first match wins, checked in
    // the order given). Useful to give containers / machines a richer panel.
    std::vector<BlockOverride> overrides;

    // Per-block-type extra info (see BlockExtrasConfig).
    BlockExtrasConfig extras;

    // --- platform specific options -------------------------------------
    ServerOptions server;
    ClientOptions client;
};

} // namespace insight
