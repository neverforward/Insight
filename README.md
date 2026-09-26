![Insight](https://socialify.git.ci/neverforward/Insight/image?custom_description=A+lightweight+info+display+mod+for+Bedrock+Edition+that+shows+you+exactly+what+you+are+looking+at.&description=1&font=Inter&forks=1&issues=1&logo=https%3A%2F%2Fraw.githubusercontent.com%2Fneverforward%2FInsight%2Frefs%2Fheads%2Fmain%2Fassets%2Ficon.svg&name=1&owner=1&pattern=Plus&pulls=1&stargazers=1&theme=Auto)

![Static Badge](https://img.shields.io/badge/English-inactive?style=for-the-badge)
[![Static Badge](https://img.shields.io/badge/简体中文-informational?style=for-the-badge)](README.zh.md)
![GitHub License](https://img.shields.io/github/license/neverforward/Insight?style=for-the-badge)
![GitHub Tag](https://img.shields.io/github/v/tag/neverforward/Insight?style=for-the-badge)
![GitHub Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/neverforward/Insight/build.yml?style=for-the-badge)
![GitHub commit activity](https://img.shields.io/github/commit-activity/y/neverforward/Insight?style=for-the-badge)

 **info display** mod for Minecraft Bedrock Edition (LeviLamina 26.40.*): it shows what you are looking at in real time - name, type, coordinates, distance, container contents, block states, ...


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
/insight gui             open the configuration screen (client only; the server has no screen yet)
/insight reload          re-read the configuration file (OP / operator)
/insight set <option> <value>   change a setting in game and save it (OP / operator)
```

`<option>` is completed by the game: the candidates are exactly the configuration names
(`format`, `maxWidth`, `extras.chest`, ...). A bad value reports the accepted ones, for example:

```
/insight set channel actionbar
/insight set maxDistance 24
/insight set format {blockName} {x} {y} {z}
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
- both bindings show up in the game's own key settings and can be remapped there - the config
  values `keyOpenConfig` / `keyToggleShow` are only the defaults (Windows virtual-key codes,
  `0` disables a binding);
- each row shows the current value and opens an inline editor when clicked, the right column holds
  a live preview of the panel plus the appearance settings, and every change is saved immediately
  (the footer reports what happened);
- while the screen is open the game does not receive keyboard/mouse input.


```jsonc
{
    "version": 4,                  // schema version; older files are merged automatically

    "enabled": true,               // master switch
    "enabledByDefault": true,      // default for players; they can toggle it with /insight

    "maxDistance": 16.0,           // maximum ray distance (blocks)
    "intervalTicks": 4,            // sampling interval (20 ticks = 1s; 4 = 0.2s)
    "passThroughLiquids": true,    // look through water / lava
    "showEmpty": false,            // keep showing something when aiming at air / beyond range
    "emptyText": "",               // text used while showEmpty is true

    "format": "{blockName}\n§7{blockType} §8· §7{x}, {y}, {z}\n{extras}",  // display format

    "overrides": [                 // per-block-type format overrides (type id substring, first match wins)
        { "match": "minecraft:chest", "format": "{blockName}\n§eChest\n{extras}" }
    ],

    "extras": {                    // per-block extras (shown where {extras} appears)
        "enabled": true,           // master switch
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
    "entityFormat": "{entityName}\n§7{entityType} §8· §7{health}/{maxHealth}",

    "server": {
        "channel": "actionbar"     // none | actionbar | tip | popup | jukebox | system | chat
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

### format placeholders

| Placeholder | Meaning | Example |
| --- | --- | --- |
| `{blockType}` | block type id | `minecraft:stone` |
| `{blockName}` | localized block name | `Stone` / `石头` |
| `{blockKey}` | translation key | `tile.stone.stone` |
| `{x}` `{y}` `{z}` | integer block coordinates | `10` |
| `{dist}` | distance (blocks) | `3.5` |
| `{dim}` | dimension | `overworld` |
| `{direction}` | facing of the block (empty when it has no facing state) | `north` / `北`, `up` / `上` |
| `{light}` | light level at that position, 0-15 (empty when unavailable) | `12` |
| `{emission}` | light emitted by the block **itself**, 0-15 | `15` (glowstone) / `0` (stone) |
| `{extras}` | per-block extra lines (joined automatically, empty when there is no data) | `Items 12/27` |

Colours: use `§` codes directly, or `&` codes (`&a &l &r`, ...); `&&` is a literal `&`.
On server channels `§` codes work natively; the client panel supports `§0-9a-f` colours and `§r`
reset, while modifier codes such as `§l/k/m/n/o` have no effect there.

#### Extras

Put `{extras}` anywhere in `format` to append the extra lines (middle or end, either is fine).

| Switch | Contents |
| --- | --- |
| `extras.chest` | container occupancy: `Items 12/27` (chest/trapped chest/barrel/hopper/dropper/dispenser/copper chest/shulker box/crafter/chiseled bookshelf/lectern/decorated pot, plus chest minecart, hopper minecart and boat with chest) |
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
