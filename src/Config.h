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
    //   "actionbar" (default) | "tip" | "jukebox" | "system" | "chat" | "none"
    // "popup" is deliberately not offered: it drops the client connection (see CHANGELOG).
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

    // How the hit points are drawn (client only: a server-side channel carries text
    // to the vanilla UI, and the server build has no way to put a sprite in it):
    //   "bar"           - one bar per row, filled in proportion to the value, painted by
    //                     the UI renderer in colors.health / colors.value
    //   "bar+number"    - the same bar, with the line's own number behind it
    //   "hearts"        - the game's own heart sprites (armor: armor sprites)
    //   "hearts+number" - the same hearts, with the number behind them
    //   "number"        - the plain "current/max" text, exactly what the server shows
    // The bar has no upper limit, so `heartsThreshold` does not apply to it; a subject
    // whose *maximum* health is above the threshold keeps the numbers in the heart
    // modes, so a boss never turns into a wall of hearts while a player or a zombie
    // does get them. The threshold counts hit points and one heart is two of them;
    // `heartsPerRow` hearts fit on a row before the next row starts above it,
    // overlapping the row below by half a heart (in the bar modes it is the bar's width,
    // measured in hearts).
    std::string healthStyle = "hearts";
    float       heartsThreshold = 40.0f;
    int         heartsPerRow    = 10;

    // Show one decimal on the hit points. The engine keeps a fractional value
    // behind the integer one, so "19.5" is a real reading rather than rounding
    // noise. Only the number form is affected - neither a sprite nor a bar can show
    // half a point more precisely than half a heart.
    bool healthDecimals = false;

    // Show the armor value at all, drawn in the same style as the hit points. An
    // entity that has no armor keeps the number line, so this cannot make a line
    // disappear.
    bool armor = true;

    // --- extra lines ------------------------------------------------------
    bool extras = true; // per-entity adapters (equipment, ...); the interface is in
                        // place, the adapters themselves come later
};

// Colours of the individual pieces of the panel, one vanilla formatting code per
// piece: a single letter out of "0"-"9" / "a"-"f" (or empty for "leave it in the
// client's plain text colour"). The "§" is not part of the value - what is stored
// is exactly the letter a player types after it - so a scheme can be read and
// edited in the config file without escape characters.
//
// These apply to both the block and the entity panel, and to the extras lines as
// well: every "label value" line a block or entity adapter produces uses `label`
// and `value`. The defaults only spell out the colours the coordinates are
// usually drawn in; anything else keeps the grey/white the panel has always used,
// so a colour scheme is opt-in.
struct ColorOptions {
    std::string name           = "f"; // the target's name on the first line
    std::string facing         = "7"; // "(north)" behind the name
    std::string identifier     = "7"; // the type id on the second line
    std::string translationKey = "7"; // "(tile.stone.stone)" behind the type id
    std::string x              = "c"; // position: x (red)
    std::string y              = "a"; // position: y (green)
    std::string z              = "b"; // position: z (aqua)
    std::string distance       = "7"; // "[12.3]" behind the position
    std::string label          = "7"; // "label value" lines: the label
    std::string value          = "f"; // "label value" lines: the value
    std::string health         = "c"; // the entity's hit points
};

// Extra info adapters, for blocks and for entities alike. Every adapter has its own
// switch, so a line a player does not care about can be silenced without losing the
// others; `enabled` gates all of them. Data availability differs by platform:
// containers and block entities exist server-side and in a local (single player)
// client world; on a client connected to a remote server only local world data is
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

    // How a container's contents are shown, client only (a server-side channel carries
    // text and nothing else, so there the "Items n/m" line stays):
    //   true  - the items themselves, drawn with the game's own item icons: one icon per
    //           non-empty slot, its stack size as a small number in the corner, laid out
    //           `chestIconsPerRow` per row and at most `chestIconLimit` of them (0 = all,
    //           with the ones left out counted as a small "+n"). The numbers are the same
    //           source as the line they replace, so an open container screen updates both.
    //   false - the plain "Items n/m · total" line
    // An empty container has nothing to draw and keeps the plain line either way.
    bool chestIcons       = true;
    int  chestIconsPerRow = 9;  // 1..27
    int  chestIconLimit   = 18; // 0 = every item, otherwise at most this many icons
    // Merge like terms before drawing: one icon per item type with the counts of its stacks
    // added up (six swords read as one sword with "6", five stacks of 64 grass blocks as
    // "320") instead of one icon per slot.
    bool chestIconsMerge = true;

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

// Extra info adapters that only mean something for an entity (the block ones are in
// BlockExtrasConfig). `extras.enabled` still gates all of them; every adapter here has
// its own switch on top of that. What a target does not have contributes nothing - a
// sheep has no owner line, an adult animal no cooldown - so a switch never leaves an
// empty line behind.
struct EntityExtrasConfig {
    // Who tamed this animal, by name ("Owner Steve"). Only tamed animals have one.
    bool owner = true;

    // The mob's active status effects ("Effects Speed II 0:12 · Strength I 0:30"),
    // read from the engine's own effect list. The name comes from the game's effect
    // table and is localized; the level is the vanilla roman numeral.
    bool effects = true;
    // How many effects fit on that one line; the rest are counted as "+n". 0 = all.
    int effectsLimit = 4;

    // Time left on an animal's breeding timer, whenever one is running: the love
    // timer of a fed animal, or the cooldown after it bred.
    bool breeding = true;

    // How long a zombie villager still needs to turn into a villager ("Converting
    // 2:31"); shown only while it is actually converting.
    bool conversion = true;

    // Armour worn by a pet or a mount - a wolf's wolf armour, a horse's horse
    // armour, the body slot in general - which is *not* one of the four humanoid
    // armour slots the equipment lines report:
    //   "all"     - show it whenever it is worn
    //   "damaged" - only once it has taken damage (durability below its maximum)
    //   "off"     - never
    std::string petArmor = "all";

    // A horse's own numbers: coat and markings, whether it is saddled, and the raw
    // movement speed / jump strength attribute values, which are what every other
    // tool reports as well.
    bool horse = true;

    // An armour stand's pose index, when it is not the default pose. The engine
    // exposes the index, not the pose names ("Athena", ...), so the index is shown.
    bool armorStand = true;

    // A villager's profession and trade level ("Profession Librarian · Level 2").
    // Both are synced actor data - the client's own render controller reads the same
    // two values - and the profession name comes from the game's own
    // "entity.villager.*" translation keys, so it follows the player's language.
    bool villager = true;

    // Whether a copper golem has been waxed. This one lives in the entity's property
    // subsystem, and the game declares that property as *not* synced to clients: on a
    // client install it therefore reads its default and contributes no line at all,
    // while the server build, which owns the state, shows it.
    bool waxed = true;
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
    // 5 -> 6: the per-field colours (ColorOptions, `colors`). 6 -> 7: how the entity
    // panel draws the hit points and the armor row (entity.healthStyle, its
    // threshold, row size and decimals, and entity.armor). 7 -> 8: the entity extras
    // (EntityExtrasConfig, `entityExtras`). 8 -> 9: a container's contents as item icons
    // (extras.chestIcons, its row size and limit). 9 -> 10: merging like terms on that row
    // (extras.chestIconsMerge).
    int version = 10;

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

    // Colour of every piece of the panel (see ColorOptions). Both panels and the
    // extras lines use it, so one scheme covers the whole display.
    ColorOptions colors;

    // Per-block-type extra info (see BlockExtrasConfig).
    BlockExtrasConfig extras;

    // Per-entity extra info (see EntityExtrasConfig).
    EntityExtrasConfig entityExtras;

    // --- platform specific options -------------------------------------
    ServerOptions server;
    ClientOptions client;
};

} // namespace insight
