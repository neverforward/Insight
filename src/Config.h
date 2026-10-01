#pragma once

#include <string>

namespace insight {

// ---------------------------------------------------------------------------
// Insight - a Jade-like "what am I looking at" display mod for Minecraft
// Bedrock (LeviLamina). This struct is the whole configuration. It is
// (de)serialized to `plugins/Insight/config/config.json` (server) or
// `mods/Insight/config/config.json` (client) through ll::config, which uses
// aggregate reflection: keep this file a plain aggregate of primitives,
// std::string / std::vector / nested plain aggregates.
// ---------------------------------------------------------------------------

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
    //   "top_left" "top_center" (default) "top_right"
    //   "middle_left" "center" "middle_right"
    //   "bottom_left" "bottom_center" "bottom_right"
    std::string anchor = "top_center";

    // Extra offset for the anchor, as a fraction of the screen size (0.05 == 5%
    // of the screen). A positive value always moves the panel away from the edge
    // it is anchored at, towards the middle of the screen (for centre anchors:
    // right/down).
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

    // Fade the panel in and out when the info display is switched on or off, in
    // seconds; the box also grows or shrinks over this time when the subject changes
    // (the text itself is swapped at once - fading it flickered on every change). 0
    // keeps the old behaviour of appearing, resizing and disappearing instantly.
    // Client only: a server-side channel hands text to the vanilla UI, which
    // cannot be animated.
    float transitionTime = 0.1f;

    // Language of the interface and of the localized block names on the *client*:
    // "zh_cn", "en", or "auto". It is a stored preference rather than something
    // detected at runtime - this client answers "en_US" even on a Chinese
    // installation, so "auto" only follows it when it reports a language this mod
    // actually ships.
    std::string language = "zh_cn";

    // Overlay behaviour while connected to a remote (multiplayer) server:
    //   "on" (default): always draw the local overlay; online only data the
    //         client itself can see is shown, and a server-side Insight may
    //         push its own channel on top of it.
    //   "off": auto-hide the local overlay when playing online - container
    //         contents / furnace data / entity stats are server-authoritative
    //         and cannot be read from the client anyway.
    std::string overlayOnRemote = "on";

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

// What the info display shows, one switch per field. This replaces the format /
// entityFormat templates and the per-block-type format overrides: the mod now
// assembles the lines itself, so a switch can never leave a stray placeholder, a
// dangling separator or a blank line in the panel.
//
// The panel is laid out as:
//   <name>§7(<facing>)
//   §7<type id>(<translation key>)
//   <x, y, z> §7<distance>
//   §7<label> §f<value> ...                light level, light emission
//   <extras>
// Every part is optional, and the parts a target does not have (a facing on a block
// whose states we do not understand, a light level we could not read) are skipped -
// the brackets and the spaces only ever appear around parts that are there.
struct DisplayOptions {
    // --- first line: name and facing --------------------------------------
    bool name   = true; // localized display name (block name / entity name)
    bool facing = true; // facing / axis of the block, drawn as "§7(north)", only when
                        // the block has a facing state we understand

    // --- second line: type id and translation key -------------------------
    bool identifier     = true;  // type id, e.g. "minecraft:stone"
    bool translationKey = false; // translation key in brackets, e.g. "(tile.stone.stone)"

    // --- third line: position and distance --------------------------------
    bool position = true;  // x, y, z
    bool distance = false; // distance to the target, in blocks

    // --- fourth line: stats ------------------------------------------------
    bool light    = true; // light level at the block
    bool emission = true; // light the block itself emits

    // --- extra lines ------------------------------------------------------
    bool extras = true; // the per-block-type adapters (see BlockExtrasConfig)
};

// Entities have their own switches and their own layout (see renderEntityText):
//   <name>§7(<facing>)
//   §7<type id>(<translation key>)
//   <x, y, z> §7<distance>
//   §7<label> §f<value>                    hit points
//   <extras>
// Light level and light emission are deliberately not here: they describe a position
// in the world, not the entity under the crosshair (a mob standing on glowstone is
// not itself emitting light).
struct EntityOptions {
    // --- first line: name and facing --------------------------------------
    bool name   = true; // player real name / name tag / localized type name
    bool facing = true; // the entity's own yaw as a compass direction, "§7(north)"

    // --- second line: type id and translation key -------------------------
    bool identifier     = true;  // entity type id, e.g. "minecraft:zombie"
    bool translationKey = false; // the key the name is resolved from, e.g. "(entity.zombie)"

    // --- third line: position and distance --------------------------------
    bool position = true;  // x, y, z of the entity
    bool distance = false; // distance to the target, in blocks

    // --- fourth line: hit points ------------------------------------------
    bool health = true; // "current/max"; hidden for entities without health

    // --- extra lines ------------------------------------------------------
    bool extras = true; // per-entity adapters (equipment, ...); the interface is in
                        // place, the adapters themselves come later
};

// Per-block extra info adapters. Every adapter has its own switch, so a line a
// player does not care about can be silenced without losing the others;
// `enabled` gates all of them. Data availability differs by platform: containers
// and block entities exist server-side and in a local (single player) client
// world; on a client connected to a remote server only local world data is
// visible, so some adapters may silently show nothing there.
struct BlockExtrasConfig {
    bool enabled = true; // master switch for every adapter below

    // --- every block ------------------------------------------------------
    bool hardness        = true; // breaking time
    bool blastResistance = true; // explosion resistance

    // --- containers -------------------------------------------------------
    bool chest     = true; // chest, barrel, hopper, dispenser, shulker box, ...
    bool bookshelf = true; // chiseled bookshelf books
    bool shelf     = true; // shelf contents
    bool lectern   = true; // lectern book and page
    bool pot       = true; // decorated pot item and sherds
    bool brewing   = true; // brewing stand slots
    bool furnace   = true; // furnace / blast furnace / smoker slots

    // --- block entities ---------------------------------------------------
    bool jukebox   = true; // record being played
    bool sign      = true; // sign text
    bool banner    = true; // banner patterns
    bool itemFrame = true; // framed item
    bool flowerPot = true; // flower pot plant
    bool painting  = true; // which painting an entity shows
    bool piston    = true; // piston state

    // --- redstone ---------------------------------------------------------
    bool redstone      = true; // wire, plates, levers, ...: signal strength
    bool repeater      = true; // repeater delay and signal
    bool comparator    = true; // comparator signal
    bool dispenser     = true; // dispenser / dropper state
    bool candle        = true; // candles: how many, lit or not
    bool respawnAnchor = true; // charge level

    // --- everything else --------------------------------------------------
    bool misc = true; // remaining block states (composter, cake, sea pickles, ...)
};

struct Config {
    // Config schema version. IMPORTANT: bump this whenever a field is added,
    // removed or renamed. LeviLamina only merges the current defaults into an
    // existing file when this number differs from the file's "version", so a
    // new field without a bump makes deserialization fail ("missing required
    // field") and the mod refuses to load. 1 -> 2: added the client key
    // bindings (keyOpenConfig / keyToggleShow). 2 -> 3: every extras adapter got
    // its own switch (BlockExtrasConfig). 3 -> 4: client.transitionTime (panel
    // fade). 4 -> 5: removed format / entityFormat / the per-block-type format
    // overrides in favour of the per-field switches in DisplayOptions (display.*).
    int version = 5;

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

    // Entity target info: shown at all when this is on, and which parts of it appear
    // is up to EntityOptions (entity.*) - blocks and entities no longer share a layout.
    bool entityEnabled = true;

    // Which lines the info display shows. Blocks and entities have separate switch
    // sets and separate layouts (see DisplayOptions / EntityOptions) - this is what
    // the player configures instead of the old text templates.
    DisplayOptions display;
    EntityOptions  entity;

    // Per-block-type extra info (see BlockExtrasConfig).
    BlockExtrasConfig extras;

    // --- platform specific options -------------------------------------
    ServerOptions server;
    ClientOptions client;
};

} // namespace insight
