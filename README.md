![Insight](https://socialify.git.ci/neverforward/Insight/image?custom_description=A+lightweight+info+display+mod+for+Bedrock+Edition+that+shows+you+exactly+what+you+are+looking+at.&description=1&font=Inter&forks=1&issues=1&logo=https%3A%2F%2Fraw.githubusercontent.com%2Fneverforward%2FInsight%2Frefs%2Fheads%2Fmain%2Fassets%2Ficon.svg&name=1&owner=1&pattern=Plus&pulls=1&stargazers=1&theme=Auto)

![Static Badge](https://img.shields.io/badge/English-inactive?style=for-the-badge)
[![Static Badge](https://img.shields.io/badge/简体中文-informational?style=for-the-badge)](README.zh.md)
![GitHub License](https://img.shields.io/github/license/neverforward/Insight?style=for-the-badge)
![GitHub Tag](https://img.shields.io/github/v/tag/neverforward/Insight?style=for-the-badge)
![GitHub Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/neverforward/Insight/build.yml?style=for-the-badge)
![GitHub commit activity](https://img.shields.io/github/commit-activity/y/neverforward/Insight?style=for-the-badge)

 **info display** mod for Minecraft Bedrock Edition (LeviLamina 26.51.*): it shows what you are looking at in real time - name, type, coordinates, distance, container contents, block states, ...


## Installation

- Server: `lip install github.com/neverforward/Insight`
- Client: `lip install github.com/neverforward/Insight#client`

The first start creates the configuration file `plugins/Insight/config/config.json` (server) or
`mods/Insight/config/config.json` (client) with every default value. Run `/insight reload` (OP) to
hot-reload it, or change values in game with `/insight set <option> <value>`.

## Commands

The command is `/insight` on the server and **`/cliinsight`** in the client build. Joining a world
merges the server's command list into the client registry and lets client mods register on top of it,
so an unprefixed client-side `/insight` would collide with a server-side one when both installs are
present - the `cli` prefix keeps them apart (the same convention as LeviLamina's own `/levilamina` vs
`/clilevilamina`). The tree below is identical on both sides.

```
/insight toggle          switch the display on/off (server: your own switch, kept in the mod's data
                         directory; client: the overlay switch, saved to the config file)
/insight on | off        same as above
/insight status          show the current state (switch/interval/distance/channel, or anchor/display/language/extras)
/insight gui             open the configuration: the client's own screen, or a form the server
                         pushes to the player who asked
/insight reload          re-read the configuration file (OP / operator)
/insight set <option> <value>   change a setting in game and save it (OP / operator)
```

`<option>` is completed by the game: the candidates are exactly the configuration names
(`display.name`, `entity.health`, `maxWidth`, `extras.chest`, ...). `display.health` no longer exists (health is
`entity.health` now) and there is no `display.dimension` at all. A bad value reports the accepted
ones, for example:

```
/insight set channel actionbar
/insight set maxDistance 24
/insight set display.distance true
/insight set entity.health false
/insight set extras.chest false
```

On the server each player's switch is stored in a key-value database inside the mod's data
directory (`plugins/Insight/data/players/`), so it survives server restarts; an entry is only
written when a player changes the switch away from its **default** value.

## Configuration

### Configuration screen (client)

The client can also edit the configuration in a screen instead of the chat:

- open it with `/cliinsight gui` or the hotkey (default <kbd>I</kbd>);
- a second hotkey (default <kbd>K</kbd>) shows/hides the info display;
- the hotkeys only act where they cannot take a key away from something else: the game has to be
  playing (no menu is up - a container, the pause screen or the chat all keep the key to themselves)
  and no text box may be selected for typing (chat, a sign, an anvil name, the creative search
  field). While the configuration screen itself is open they belong to it, except while it waits for
  a key to rebind or an edited field has the caret;
- both bindings show up in the game's own key settings and can be remapped there - the config
  values `keyOpenConfig` / `keyToggleShow` are only the defaults (Windows virtual-key codes,
  `0` disables a binding);
- each row shows the current value and opens an inline editor when clicked; the left column is
  split into five tabs - **General**, **Block lines**, **Entity info**, **Extras** and **Keys** -
  instead of one long column. **Block lines** holds the block switches grouped by the line they
  feed: **Title line** (Name, Facing), **Details line** (Type id, Translation key), **Position
  line** (Position, Distance), **State line** (Light, Emission) and **Extras** (Extras lines ->
  `display.extras`). **Entity info** holds **Entity info** (`entityEnabled`) and the entity
  switches in the same grouping: **Title line** (Name, Facing), **Details line** (Type id,
  Translation key), **Position line** (Position, Distance), **State line** (Health) and
  **Extras** (Extras lines -> `entity.extras`). **Extras** holds **Extras** (`extras.enabled`),
  **Extras: every block**, **Extras: containers**, **Extras: block entities**, **Extras:
  redstone** and **Extras: block states**, and **Keys** holds the two key bindings. The right
  column still holds a live preview of the panel plus the appearance settings; every change is
  saved immediately (the footer reports what happened);
- while the screen is open the game does not receive keyboard/mouse input.

### Configuration menu (server)

A server has no screen of its own, so `/insight gui` pushes a **form** to the player who asked: a group
picker first, then one form per group whose submit button applies everything at once. The values are
written through the same path as `/insight set`, so the range checks, the validation and the immediate
save behave exactly like the command - a value that is out of range says why in chat, and the menu
reopens so another group can be edited. The groups are **General** (the master switch, distance,
interval, the channel, ...), **Block lines**, **Entity lines**, **Colours**, **Extras: every block**,
**Extras: block entities** and **Extras: redstone**.

The client-only options are deliberately absent from the menu - `showOverlay`, the anchor, the offsets,
the font and appearance settings, the language and the key bindings do nothing on a server.
`.cache/check_gui_options.py` keeps that claim honest: it checks that the menu offers every option
`/insight set` accepts on a server, and nothing else, and that every group title and row label it shows
is translated.


```jsonc
{
    "version": 7,                  // schema version; older files are merged automatically, and the
                                   // templates of a version <= 4 file become display / entity
                                   // switches (see below)

    "enabled": true,               // master switch
    "enabledByDefault": true,      // default for players; they can toggle it with /insight

    "maxDistance": 16.0,           // maximum ray distance (blocks)
    "intervalTicks": 4,            // sampling interval (20 ticks = 1s; 4 = 0.2s)
    "passThroughLiquids": true,    // look through water / lava
    "showEmpty": false,            // keep showing something when aiming at air / beyond range
    "emptyText": "",               // text used while showEmpty is true

    "display": {                   // what the block panel shows, one switch per part (replaces the old text templates)
        "name": true,                  // title line: localized block name
        "facing": true,                // title line: the block's facing / axis in brackets after the name, e.g. Stone§7(north); skipped when the block has no facing state we understand
        "identifier": true,            // details line: block type id, e.g. minecraft:stone
        "translationKey": false,       // details line: block translation key in brackets after the type id, e.g. minecraft:stone(tile.stone.stone)
        "position": true,              // position line: x, y, z
        "distance": false,             // position line: distance to the block in blocks
        "light": true,                 // state line: light level at the block, e.g. Light 12
        "emission": true,              // state line: light the block itself emits, e.g. Emission 15
        "extras": true                 // per-block-type extra lines (gated by extras.enabled too)
    },

    "entity": {                    // what the entity panel shows (entityEnabled decides whether entities are targeted at all)
        "name": true,                  // title line: player real name / name tag / localized type name
        "facing": true,                // title line: the entity's own yaw as a compass direction, in brackets after the name
        "identifier": true,            // details line: entity type id, e.g. minecraft:zombie
        "translationKey": false,       // details line: the key the name is resolved from, in brackets after the type id, e.g. minecraft:zombie(entity.zombie)
        "position": true,              // position line: x, y, z of the entity
        "distance": false,             // position line: distance to the entity in blocks
        "health": true,                // state line: current/max hit points; hidden for entities without health (items, projectiles, paintings)
        "healthStyle": "hearts",       // how the hit points (and the armor row) are drawn, client only: bar | bar+number | hearts | hearts+number | number
        "heartsThreshold": 40.0,       // hit points; in hearts mode a subject with more maximum health than this keeps the numbers instead
        "heartsPerRow": 10,            // hearts per row (bar mode: how wide the bar is, measured in hearts); every further row starts above the one before and overlaps it by half a heart
        "healthDecimals": false,       // one decimal on the hit points in number mode, e.g. 19.5/20
        "armor": true,                 // show the armor row, drawn in the same style as the hit points
        "extras": true                 // per-entity extra lines (the same extras.* adapters as the block side, gated by extras.enabled too)
    },

    "colors": {                    // colour of every part of the panel: one formatting code per part,
                                   // without the "§" ("c" = §c). "" (or none / off / default) leaves
                                   // that part in the plain text colour. Both panels and the extras
                                   // lines use these, so one scheme covers the whole display.
        "name": "f",               // the target's name
        "facing": "7",             // the facing in brackets after the name
        "identifier": "7",         // the type id line
        "translationKey": "7",     // the translation key in brackets after the type id
        "x": "c",                  // position: x (red)
        "y": "a",                  // position: y (green)
        "z": "b",                  // position: z (aqua)
        "distance": "7",           // the distance in brackets behind the position
        "label": "7",              // "label value" lines: the label
        "value": "f",              // "label value" lines: the value
        "health": "c"              // the entity's hit points
    },

    "extras": {                    // extras adapters, shared by the block and the entity panel (shown while display.extras / entity.extras is on)
        "enabled": true,           // master switch for every adapter on both sides
        "hardness": true,          // breaking time of every block
        "blastResistance": true,   // explosion resistance
        "chest": true,             // container occupancy: items 12/27
        "bookshelf": true,         // chiseled bookshelf books
        "shelf": true,             // shelf contents
        "lectern": true,           // lectern book + page
        "pot": true,               // decorated pot item + sherds
        "brewing": true,           // brewing stand slots
        "furnace": true,           // furnace / blast furnace / smoker slots
        "jukebox": true,           // record being played
        "sign": true,              // sign text
        "banner": true,            // banner patterns
        "itemFrame": true,         // framed item
        "flowerPot": true,         // flower pot plant
        "painting": true,          // which painting is hanging there
        "piston": true,            // piston state
        "redstone": true,          // wire, plates, levers, ... strength
        "repeater": true,          // repeater delay + signal
        "comparator": true,        // comparator signal
        "dispenser": true,         // dispenser / dropper state
        "candle": true,            // candles: how many, lit or not
        "respawnAnchor": true,     // charge level
        "misc": true               // remaining block states
    },

    "entityEnabled": true,         // also show entities under the crosshair

    "server": {
        "channel": "actionbar"     // none | actionbar | tip | jukebox | system | chat
    },

    "client": {
        "showOverlay": true,
        "anchor": "top_center",    // top_left/top_center/top_right/middle_left/center/middle_right/bottom_left/bottom_center/bottom_right
        "offsetX": 0.0,            // horizontal offset as a fraction of the screen width (positive = inward from the anchor)
        "offsetY": 0.0,            // vertical offset as a fraction of the screen height (positive = inward from the anchor)
        "fontSize": 1.0,           // font scale
        "background": true,        // translucent black panel behind the text
        "backgroundAlpha": 0.45,   // panel opacity 0..1
        "shadow": true,            // text shadow
        "textColor": "ffffff",     // text colour (RRGGBB) when no colour code is used
        "maxWidth": 0.0,           // max panel width as a fraction of the screen, 0 = unlimited (long lines wrap)
        "transitionTime": 0.1,     // seconds to fade the panel in/out (display toggled) and to resize it (target changed), 0 = instant
        "hideOverlayInGui": true,  // hide the panel while an inventory / container screen is open
        "overlayOnRemote": "on",   // on = always draw the local panel; off = hide it online (the server pushes it)
        "language": "zh_cn",       // zh_cn, en, or auto (follow the client when it reports a shipped language)
        "keyOpenConfig": 73,       // hotkey for the configuration screen (VK code, 0 = disabled)
        "keyToggleShow": 75        // hotkey to show/hide the info display (VK code, 0 = disabled)
    }
}
```

> For detailed logs (every block state, container slot decisions, piston / decorated pot
> diagnostics, ...) set `logLevel` to `5` in `PreLoaderConfig.json`.

### display switches (blocks)

There is no text template to keep in sync any more: the mod assembles each panel itself, one switch
per part. On the client it is drawn with the game's own UI renderer (engine font, engine item
renderer for the subject icon, rounded background), and it fades in/out and resizes over
`client.transitionTime`. Blocks and entities no longer share one switch set: `display.*` below
describes a **block** panel, `entity.*` the **entity** panel. Every switch is a boolean: `true` /
`false` (`on` / `off` and `1` / `0` work too), the same parsing as every other switch.

| Switch | Default | Shows |
| --- | --- | --- |
| `display.name` | `true` | localized block name, the title line |
| `display.facing` | `true` | the block's facing / axis in muted brackets right after the name, e.g. `Stone§7(north)`; skipped when the block has no facing state we understand |
| `display.identifier` | `true` | block type id, e.g. `minecraft:stone`, the details line |
| `display.translationKey` | `false` | the block translation key in the same muted colour right after the type id, e.g. `§7minecraft:stone(tile.stone.stone)` |
| `display.position` | `true` | `x, y, z`, the position line |
| `display.distance` | `false` | distance to the block in blocks, behind the position |
| `display.light` | `true` | light level at the block, shown as `Light 12` |
| `display.emission` | `true` | light the block itself emits, shown as `Emission 15` |
| `display.extras` | `true` | the per-block-type extra lines (the `extras.*` adapters, gated by `extras.enabled` too) |

### entity switches (entities)

`entityEnabled` decides whether entities are targeted and shown at all; the switches below only
choose what an entity panel contains once one is targeted.

| Switch | Default | Shows |
| --- | --- | --- |
| `entity.name` | `true` | player real name / name tag / localized type name, the title line |
| `entity.facing` | `true` | the entity's **own yaw** as a compass direction, in muted brackets right after the name |
| `entity.identifier` | `true` | entity type id, e.g. `minecraft:zombie`, the details line |
| `entity.translationKey` | `false` | the key the name is resolved from, in the same muted colour right after the type id, e.g. `§7minecraft:zombie(entity.zombie)` |
| `entity.position` | `true` | `x, y, z` of the entity, the position line |
| `entity.distance` | `false` | distance to the entity in blocks, behind the position |
| `entity.health` | `true` | hit points as `current/max`, e.g. `§7Health §f12/20`; hidden for entities without health (items, projectiles, paintings) |
| `entity.healthStyle` | `hearts` | how the hit points and the armor row are drawn (client only, see below): `bar`, `bar+number`, `hearts`, `hearts+number` or `number` |
| `entity.heartsThreshold` | `40` | hit points; in `hearts` mode a subject whose **maximum** health is at most this is drawn as hearts, anything bigger keeps the numbers |
| `entity.heartsPerRow` | `10` | hearts on one row before the next row starts above it; in `bar` mode it is how wide the bar is, measured in hearts |
| `entity.healthDecimals` | `false` | show one decimal on the hit points in `number` mode, e.g. `19.5/20` |
| `entity.armor` | `true` | show the armor row, drawn in the same style as the hit points, e.g. `§7Armor §f15` in `number` mode |
| `entity.extras` | `true` | the per-entity extra lines (container entities such as chest / hopper minecarts and boats with chest, paintings, worn equipment); the same `extras.*` adapters as the block side, gated by `extras.enabled` too |

### colours

Every part of the panel has its own colour, taken from the `colors` block above. A value is a
**formatting code without the `§`** - exactly the character a player would type after it - so `"c"`
means `§c`. An empty string (or `none` / `off` / `default`) leaves that part in the plain text
colour, which is `client.textColor`.

The codes are the usual sixteen, plus the ones only Bedrock has:

| Code | Colour | Code | Colour | Code | Colour | Code | Colour |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `0` | black | `4` | dark red | `8` | dark gray | `c` | red |
| `1` | dark blue | `5` | dark purple | `9` | blue | `d` | light purple |
| `2` | dark green | `6` | gold | `a` | green | `e` | yellow |
| `3` | dark aqua | `7` | gray | `b` | aqua | `f` | white |

| Code | Bedrock colour | Code | Bedrock colour | Code | Bedrock colour |
| --- | --- | --- | --- | --- | --- |
| `g` | minecoin gold | `n` | copper | `t` | lapis |
| `h` | quartz | `p` | gold ingot | `u` | amethyst |
| `i` | iron | `q` | emerald | `v` | resin |
| `j` | netherite | `s` | diamond | `w` | party blue |
| `m` | redstone | | | | |

`colors.label` and `colors.value` are what the labelled lines use, so they cover the extras lines
too: a chest's `Items 12/27` is a label plus a value, not a separate setting. The codes shown
throughout this file (`§7`, `§f`, ...) are the defaults.

### hit points and armor: bar, hearts or numbers

The entity panel can draw the hit points and the armor row in three ways, chosen with
`entity.healthStyle` (and it applies to both rows):

- **`bar`** - one bar per row, painted by the game's own UI renderer: a rounded dark track with the
  fill laid over it, its width in proportion to the value. The fill takes its colour from the colour
  scheme (`colors.health` for the hit points, `colors.value` for the armor). A bar has no upper limit,
  so `heartsThreshold` does not apply; `heartsPerRow` sets how wide it is, measured in hearts.
- **`hearts`** - the game's own heart sprites, and armor sprites for the armor row. One heart is
  **two hit points** and half a heart is as fine as it gets. `entity.heartsThreshold` is compared
  against the subject's **maximum** health, not its current one, so the panel cannot flip between
  hearts and numbers while a fight is going on: a player or a zombie (20) is drawn as hearts at the
  default 40, an iron golem (100) or the ender dragon keeps the numbers. `entity.heartsPerRow` hearts
  fit on a row; every further row starts **above** the previous one and overlaps it by half a heart,
  so the upper row covers the lower one.
- **`number`** - the plain `current/max` text, which is also what a server install always shows.
- **`bar+number`** / **`hearts+number`** - the same bar or hearts with the line's own number
  (`Health 12/20`) written behind them, so a row shows both the graphic and the exact value. A row
  like this is one line of text tall, with the graphic centred against it.

Every row is exactly one line of text tall, so the panel keeps its rhythm whatever the style is. The
whole drawing is **client only**: a server-side channel carries text to the vanilla UI, and the
server build has no way to put a sprite in it.

A **server** install spells the bar out in text instead, because that is all a vanilla channel can carry:
`█` for every whole step, a left half block `▌` for half a step, and the empty rest as the same `█` in the
muted label colour - the same fixed-width track the client paints. The icon styles are deliberately not
spelled out there, because the font's own glyphs read badly in a vanilla channel: a server shows the
numbers for the heart styles, and the `+number` styles put those numbers behind the icons or the bar on
the client.

Two details worth knowing: an entity with no armor at all keeps the number line rather than showing an
empty row, and when a sprite cannot be loaded that row falls back to its number text, so a line is
never blank.

The lines those switches produce, in order. A block panel:

```
<name>§7(<facing>)                                         <- title line (display.name, display.facing)
§7<type id>(<translation key>)                             <- details line (display.identifier, display.translationKey)
<x, y, z> §7<distance>                                     <- position line (display.position, display.distance)
§7<label> §f<value> ...                                    <- state line: light level, light emission
<extras lines>                                             <- one line per adapter
```

and an entity panel:

```
<name>§7(<facing>)                                         <- title line (entity.name, entity.facing)
§7<type id>(<translation key>)                             <- details line (entity.identifier, entity.translationKey)
<x, y, z> §7<distance>                                     <- position line (entity.position, entity.distance)
§7<label> §f<current>/<max>                                <- state line: hit points (entity.health)
<extras lines>                                             <- one line per adapter
```

- the facing hangs off the **name** on line 1 in muted brackets (`§7`) - the block's own facing
  state, or the entity's own yaw; simply absent when the switch is off or the target has no facing;
- the translation key sits in brackets right after the **type id** on line 2, in the same muted
  colour;
- line 3 is the position in the default text colour followed by the distance in `§7`, separated by
  one space;
- line 4 holds the labelled stats, each as `§7<label> §f<value>` and separated by a single space:
  the block panel shows the light level and the light the block emits (`Light 12 Emission 15`), the
  entity panel the hit points (`Health 12/20`) - an entity has no light statistics;
- the `·` that used to separate every format part is gone - the panel joins its parts with single
  spaces. The one `·` left is inside a container's extras line, between the slot count and the total
  item count (`Items 12/27 §7· §f35`);
- the dimension is no longer shown anywhere.

Parts that are switched off, and parts a target does not have (a facing on a block whose states we
do not understand, health on an entity type that has none), are skipped, and empty lines are
dropped, so the layout never comes out malformed. `emptyText` is still used when `showEmpty` is on
and nothing is targeted; it no longer has placeholders.

Colours: the panel's own lines already carry `§` codes (`§7` for the muted parts - the facing, the
type id, the translation key, the distance and the stat labels - and `§f` for the values; the extras
lines use the same two, plus a dark grey `§8` for their filler glyphs such as a truncated item list's
`...` or an empty record's `-`), so there is nothing to colour yourself. The one text you write is
`emptyText`: it is drawn exactly as typed - `&` is not a colour code, so write `§` codes directly if
you want colour there.

#### Upgrading an old configuration

A `config.json` that still carries a schema version below 5 described its panel with the old
`format` / `entityFormat` text templates. Before the configuration library merges the new defaults
in and rewrites the file, the mod reads those templates once and turns them into switches, one
group at a time: the old block `format` template decides the `display.*` switches and the old
`entityFormat` template decides the `entity.*` switches, a placeholder the template used meaning
that part stays visible and a placeholder it never used meaning that switch is off. An old default
`format` that used `{blockName}`, `{blockType}`, `{x} {y} {z}` and `{extras}` therefore ends up with
`display.name`, `display.identifier`, `display.position` and `display.extras` on and the rest of the
block group off, and the same rule maps the entity template's placeholders onto their `entity.*`
switch - for example `{entityName}` -> `entity.name`, `{entityType}` -> `entity.identifier`,
`{x} {y} {z}` -> `entity.position`, `{health}` / `{maxHealth}` -> `entity.health`, `{extras}` ->
`entity.extras` - so a player who upgrades keeps the panel they had. A template that is not in the
file at all leaves its group at the defaults instead of switching everything off. The migration runs
only for a file below version 5 and the result is saved immediately. The per-block-type `overrides`
have no equivalent and are still dropped, and `{dim}` has no switch to feed either - the dimension is
not shown any more - so it is ignored along with them.

#### Extras

The extra lines are appended while the panel's own switch is on - `display.extras` for a block,
`entity.extras` for an entity - and `extras.enabled` gates every adapter on **both** sides, so it
silences block and entity extras alike. The entity panel is not a separate set of adapters: it reuses
the same `extras.*` switches (a container entity such as a chest or hopper minecart is `extras.chest`,
a painting `extras.painting`, and the worn equipment of players, armor stands and armored mobs is
`extras.misc`). Every adapter below has a switch of its own:

| Switch | Contents |
| --- | --- |
| `extras.chest` | container occupancy: `Items 12/27` (plus the total item count behind a `·`, e.g. `Items 12/27 §7· §f35`) - chest/trapped chest/barrel/hopper/dropper/dispenser/copper chest/shulker box/crafter/chiseled bookshelf/lectern/decorated pot, plus chest minecart, hopper minecart and boat with chest |
| `extras.furnace` | furnace / blast furnace / smoker: `Slots 1/3` |
| `extras.brewing` | brewing stand: `Slots x/5` |
| `extras.redstone` | comparator, redstone wire / repeater, pressure plate, target: `Signal 12` |
| `extras.misc` | block states and block-entity info, see below |

Every line below belongs to the switch named after it (`extras.hardness`, `extras.banner`,
`extras.sign`, `extras.furnace`, `extras.brewing`, ...). `extras.misc` is the master switch for the
plain block states and also owns the block-entity lines that have no switch of their own (beehives,
beacons, campfires, beds, enchanting tables). What they all show:

- Doors / trapdoors / fence gates: `State open/closed` (a `Powered` line is added while powered)
- Buttons: `State pressed/released`; levers: `State open/closed`; tripwire hooks: `Connected`, `Powered`
- Observers: `Powered yes/no`
- Pistons / sticky pistons: `State extended/retracted` (and `extending` / `retracting` while animating); piston arms: `Piston normal/sticky` + `State extended`
- Crafters: `State crafting/idle` and `Triggered yes/no` (the crafter's two own states - Bedrock spells the second one `triggered_bit`), both under `extras.misc`, plus `Disabled slots n` read from the block entity
- Sea pickles: `Count n`
- Cake: `Slices n/7`; composters: `Compost n/8`
- Furnaces / blast furnaces / smokers: `Time left Ns` (what is left of the item currently cooking) and `Cook progress N%`; only the item in the fire is reported, and the countdown walks on to the next item by itself. The engine only details a block entity to a client while its screen is open, so the client counts on from the last value it was sent: fuel running out, or a hopper refilling while nobody looks, can make it drift until the furnace is opened again
- Brewing stands: `Brewing N%` (kept counting between screen opens the same way) and `Fuel n/m`
- Bee nests / beehives: `Bees n/3` (plus `Honey level n/5` from the block state)
- Beacons: `Beacon level n`
- Campfires: `Cooking <item> Ns/30s` (time left, kept counting the same way) for every item being cooked
- Beds: `Occupied yes/no`
- Enchanting tables: `Enchant power n` (bookshelf power, capped at 15)
- Banners: `Patterns n`; decorated pots: `Items <item> ×n` and `Sherds n/4`; shelves: `Items n/3`; item frames: `Displayed item <item>` and `Rotation N°`
- Lecterns: `Book <item>`, `Page p/total`; signs: `Text <front>` and `Text (back) <back>`
- Equipment of any entity that carries some (armor stands, players, armored mobs): `Helmet` / `Chestplate` / `Leggings` / `Boots`, `Main hand`, `Off hand`

**Data completeness**: container and block-entity info is complete on the **server and in local
single-player**; a client connected to a remote server cannot read other players' containers, so
those lines stay empty instead of failing. Cooking timers (furnace, brewing stand, campfire) are
extrapolated locally from the last value the client was sent, so they keep ticking while the screen
is closed and self-correct as soon as a real value arrives. Not covered yet: command blocks, mob spawners, crop
growth stages, and the note block (its pitch and instrument are not reachable through the 26.40
API; the option was removed rather than showing a guessed value).

## Language

- Block and item names follow the **player's own language**: they are looked up in the engine's
  localization tables, which already merge the vanilla pack with every resource pack the player
  enabled, then fall back to English and finally to the raw type id.
- The panel labels and command feedback ship as `lang/en.json` and `lang/zh_cn.json` (the file name
  is the locale code, lowercase). Drop another `<locale>.json` into `lang/` to add a language;
  missing entries fall back to English and never turn into blank text.

## Building

1. Install [xmake](https://xmake.io/), clang-cl and VS2022+

2. Build the mod
```bash
# server (BDS)
xmake f -y -p windows -a x64 -m release --target_type=server
xmake

# client (GDK / LeviLamina client)
xmake f -y -p windows -a x64 -m release --target_type=client
xmake
```

The artifacts are written to `bin/Insight/` - that whole folder is what you place as described in
"Installation".

# License

MIT © neverforward
