# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

<!-- The release workflow reads the section matching the released tag from this
     file (ffurrer2/extract-release-notes), so the version heading below has to
     match the tag, e.g. tag `v1.0.0` -> `## [1.0.0] - YYYY-MM-DD`. -->

## [Unreleased]
### Added
- Text fields in the configuration screen take Chinese, Japanese and Korean input. Clicking one puts a
  native text box exactly on top of it, so the system IME works there the way it does anywhere else -
  candidate window included - and what is typed lands in the field as it goes. ImGui never sees an IME
  composition on this client (the game window produces none), which is why a plain input field could
  only take ASCII. Enter, Tab or a click elsewhere ends the edit and saves it; Esc leaves the field
  without writing.

### Changed
- The configuration screen and the server's `/insight gui` menu group their rows by what they
  describe, instead of keeping "extras" sections of their own that split one subject in two. On the
  client the Extras tab is gone: the block extras sit under **Block lines** (the container icon
  options right below `Containers`, the two duplicate "Extras: containers" groups merged into one),
  and the per-entity extras under **Entity info**. On a server the menu is down from eight groups to
  four - **General**, **Block lines**, **Entity lines**, **Colours** - with the same rows moved into
  the two new groups and the `extras.enabled` master switch in **General**, since it gates both
  halves. No option is renamed and none moved inside the file, so an existing configuration loads
  exactly as it did.
- The configuration screen is one column of tabs. The appearance settings and the colours used to
  have a column of their own beside the list, which cost half the window, so the window had to be
  larger than the panel it configures and covered it; they are an **Appearance** tab now, and the
  list has the whole width.
- The screen opens at 62% of the display, capped at 1100x780, instead of 75% - small enough to leave
  the panel in view while a setting is changed. Its size and position are applied once, on first
  open: dragging it now keeps the result, where before every frame reset the size and a resize
  snapped back as soon as the mouse was released.
- Every row of the screen carries its own control, so nothing has to be opened before it can be
  changed: a switch you click once, a slider you drag, a dropdown, or a text field, laid out in two
  columns with the label on the left and the control on the right. A row keeps what it is being given
  while it is being edited and saves when the edit ends, which is also what stops a drag from being
  undone. A small switch replaces the checkbox (`#e5e5e5` track with a `#171717` knob when on,
  `#282828` with `#fafafa` when off, no hover recolour) and sits at the right end of its row.
- The screen was restyled: one neutral palette instead of a blue accent on a warm dark grey (background
  `#09090b`, controls `#18181b`, borders `#27272a`, text `#fafafa` and `#a1a1aa`, the accent
  near-white), 1px borders, 10px radii, a segmented pill tab list in place of the tab bar's underline,
  muted captions in place of separators, and a filled primary button at the bottom.

### Removed
- The preview panel in the configuration screen. It drew its own copy of the panel with ImGui, but
  the overlay draws the real one with the game's renderer and this screen never covered it, so the
  copy only ever disagreed with what was on screen. The two messages it needed
  (`Preview`, `Nothing under the crosshair`) are gone from `lang/` with it.

### Fixed
- The panel picks the block by what that block actually looks like, not by the cell it sits in. A
  torch, a fence post, a flower, a button or a slab is now hit where its own outline is, and a
  crosshair aimed at the empty part of its cell goes past it to whatever stands behind - the ray used
  to be marched through the grid and stop at the first cell that was not air, which made every block
  a full cube. It reads the same outline shapes the game's own pick uses, and asks the hit block to
  confirm the hit, because the engine's clip can name the cell above the aimed one for some shapes.

## [0.5.0] - 2026-10-25
### Added
- A furnace, blast furnace and smoker report `Time left` and `Cook progress` again, now read from a
  world that really owns the machine: a server always, and a client in a local save (where the
  panel reads the hosted world). `Cook progress` is how far the item in the fire has got - 0% when it
  has just gone in, 100% when it is done - and `Time left` is one full smelt for every item behind it
  plus what is left of it. A client in a remote session keeps the slot line and the contents line
  only: its networked copy of a machine's counter is partial and arrives in bursts, so the two lines
  would be a guess there.
- In a world this game hosts itself - a local save - the panel reads the block out of the server
  half of the engine, which holds the complete world, instead of out of the networked copy the
  client is sent: a container's contents and a machine's counters are then what the server sees
  (a furnace that reads `1/3` holding one stack on the client now reports all three slots). The
  look is handed to the server's own tick, so the world is read on the thread that owns it; a
  remote session has no such level and keeps reading the client's world.
- A container's contents are drawn as the items themselves, not as a count. On the client the
  `Items 12/27 · 35` line is replaced by the game's own item icons - one per non-empty slot and as
  large as the game draws an item in its own GUI, the stack size written small into the icon's corner the way
  vanilla writes it, `extras.chestIconsPerRow`
  icons per row and at most `extras.chestIconLimit` of them (0 = every item, the stacks that did not
  fit are counted as a small `+n`). `extras.chestIconsMerge` (on by default) draws like terms as one
  icon with their counts added up - six swords read as one sword with `6`, five stacks of 64 grass
  blocks as `320` - instead of one icon per slot. The container's line is always the panel's last one
  now, whatever else the block reports, so the icons sit at the bottom of the box; an empty container
  has nothing to draw, so the client drops the line instead of reading `Items 0/27`, and
  `extras.chestIcons: false` brings the plain line back. The icons come from the same source as the
  numbers did, so an open container screen updates them as it is edited. A server-side channel carries
  text and nothing else, so there the line stays as it was: `Items 12/27 · 35`.

### Changed
- Higher `Config` schema version (9) for the container icon options above, and (10) for merging like
  terms on that row; an existing file is merged automatically.
- The container icon options are drawn by the client's own overlay, so on a server they change
  nothing: the server's `/insight gui` menu no longer lists them.

### Removed
- The pale one-unit outline around the info panel. The box is its rounded background fill and
  nothing else now; `client.background: false` still removes the box entirely.

### Fixed
- A furnace, blast furnace or smoker's contents follow the machine: while its screen is open they
  come from the screen's own container models, which the engine fills with the live stacks - the
  block entity's copy of them is partial or stale (a lit furnace can read `1/3` holding a single
  stack while it really has ore, fuel and output). The models that wrap the block's container are
  summed now, so the smoker and blast furnace screens are covered too: they reuse the very same
  `furnace_*` models, which matching by the screen's name prefix missed. A furnace whose screen
  was never opened still shows what its block entity has, since that is all a client is sent.
- Leaving a world and loading another one no longer carries state over into the new one: the
  container snapshots and the cooking estimates are keyed by block positions and by addresses in
  the level's memory, both of which the next world reuses, so they are dropped when the level
  changes.

## [0.4.0] - 2026-10-04
### Added
- Every part of the panel has a colour of its own. The `colors` group has one entry per part - `name`,
  `facing`, `identifier`, `translationKey`, `x`, `y`, `z`, `distance`, `label`, `value` and `health` -
  and each is a vanilla formatting code **without the `§`**: `"c"` means `§c`, and an empty value
  (or `none` / `off` / `default`) leaves that part in the plain text colour, `client.textColor`. Both
  panels and the extras lines read from the same group - a labelled line such as a chest's
  `Items 12/27` uses `colors.label` for its label and `colors.value` for its value - so one scheme
  covers the whole display. The defaults only spell out the colours the coordinates are usually drawn
  in (`x` red, `y` green, `z` aqua) plus red hit points; every other part keeps the grey/white the
  panel has always used, so a colour scheme stays opt-in.
- The accepted codes are the Bedrock set, which is larger than Java's sixteen: `g` minecoin gold,
  `h` quartz, `i` iron, `j` netherite, `m` redstone, `n` copper, `p` gold, `q` emerald, `s` diamond,
  `t` lapis, `u` amethyst, `v` resin and `w` party blue, next to the sixteen `0`-`9` / `a`-`f`. The
  formatting codes (`k`, `l`, `o`, `r`) are rejected - one of those on a line would change how the
  rest of the panel is drawn.
- The configuration screen has a **Colours** group with one dropdown per part, each option shown as
  the colour's name (Red, Dark aqua, Minecoin gold, ...) rather than its code.
  `/insight set <colors.*> <code>` takes the bare code or the `§c` / `&c` / `#c` spellings, and
  `none` clears the colour again.
- The entity panel draws the hit points and the armor row itself, and `entity.healthStyle` picks how:
  `bar`, `bar+number`, `hearts`, `hearts+number` or `number` (it covers both rows). The bar is painted
  by the game's own UI renderer - a rounded dark track with the fill laid over it - and takes its colour
  from the scheme (`colors.health` for the hit points, `colors.value` for the armor). The hearts are the
  vanilla sprites (`heart_new`, falling back to `heart`), with the background sprite and the half sprite
  over it for a half heart, and every row is exactly one line of text tall. A row is drawn for the
  subject's whole scale, not for what is left of it, so hearts that have been lost stay visible as their
  background sprites.
- `entity.heartsThreshold`, `entity.heartsPerRow`, `entity.healthDecimals` and `entity.armor`: how many
  points still fit as icons (a subject whose _maximum_ health is above the threshold keeps the numbers,
  so a boss does not turn into a wall of hearts while a player or a zombie does get them), how many
  hearts fit on a row before the next one starts above it and overlaps the row below by half a heart
  (in the bar styles it is the bar's width instead), whether the hit points carry one decimal, and
  whether the armor row appears at all.
- On a **server** install the bar is spelled out in text, because that is all a vanilla channel
  carries: `█` per whole step, `▌` for half a step and the empty rest as the same `█` in the muted
  label colour - the same fixed-width track the client paints, red for the hit points by default. The
  icon rows are deliberately _not_ spelled out there: the font's glyph hearts read badly in a vanilla
  channel, so a server shows the numbers for those styles.
- `/insight gui` opens a configuration **menu on the server** as well, since a server has no screen of
  its own: it pushes a group picker followed by one form per group to the player who asked. The values
  go through the same path as `/insight set`, so the range checks, the validation and the immediate
  save behave exactly like the command - a value that is out of range says why in chat and the menu
  reopens. The groups are **General** (the master switch, distance, interval, the channel, ...),
  **Block lines**, **Entity lines**, **Colours**, **Extras: every block**, **Extras: block entities**
  and **Extras: redstone**, which covers every option that means something on a server.
- The entity panel grew its own group of extra lines, `entityExtras` (`extras.enabled` gates them like
  the block ones), each showing what the subject actually has and staying silent otherwise:
  the **owner** of a tamed animal, the active **status effects** with their remaining time on one line
  (`effectsLimit` caps how many fit), a **breeding** timer (the love timer or the cooldown after
  breeding), a zombie villager's **conversion** time, the **pet armour** in the body slot
  (`all` / `damaged` / `off` - the humanoid armour slots were already reported), a **mount**'s coat and
  markings, movement speed, jump strength and saddle, an **armour stand**'s pose index, a
  **villager**'s profession and trade level, and whether a **copper golem** is waxed.
- A horse's coat and markings are named in the order the game's own horse render controller lists its
  textures in (`Array.base` / `Array.markings`, indexed by `query.variant` / `query.mark_variant`), so
  the same seven colours and five markings the vanilla horse models use; a horse with no markings shows
  the coat alone.
- Where those values come from: the status effects, breeding and conversion state are the engine's own
  components, the timers are ticks; a villager's profession and trade level are synced actor data - the
  game's own villager render controller reads the same two values as `query.variant` and
  `query.trade_tier` - and the profession names are the game's `entity.villager.*` translation keys, so
  they follow the player's language. Two of them are server-side state that the game never syncs to a
  client: the copper golem's waxed flag (an entity _property_ the game declares as not client-synced) and
  a zombie villager's conversion _countdown_ (only the "it is converting" flag is synced), so those lines
  appear on a server install and stay empty on a client.
- Two of the requested entity adapters are **not** implemented, because the engine does not expose what
  they need: a baby's **growth time** (its component carries a raw age counter with no total and no
  documented unit or sign, and it is not synced) and a **villager's profession as a name on the old
  villager type** (only the V2 villager's variant is the profession; the old one has no such field).
  The **next entity drop** is deliberately left out as well: asking the loot table would consume the
  world's random number generator, which is not something a display mod may do.

### Changed
- Higher `Config` schema version (6) for the `colors` group; an existing file is merged
  automatically and keeps every value it already has.
- Higher `Config` schema version (7) as well, for the entity display options above; an existing file
  is merged automatically.
- Higher `Config` schema version (8) for the `entityExtras` group above; an existing file is merged
  automatically.
- In the `+number` styles the client writes the row's own "label number" text behind the sprites,
  never the line itself: that line may carry a bar spelled out in block characters for a server
  channel, which must not appear next to the sprites.
- The `gui` subcommand is registered on both sides now (it used to exist on the client only) and is
  always opened for the player who asked: the client shows its own screen, the server pushes the menu.
  A console origin is told that the command can only be run by a player. The client-only options are
  absent from the server's menu - `showOverlay`, the anchor, the offsets, the font and appearance
  settings, the language and the key bindings do nothing there.

### Removed
- The `server.channel: "popup"` value is gone. A popup sent as a text packet made the client
  reject it and drop the connection (the "Block" error on join, "There was an issue with the
  Minecraft data being sent or received"), so the channel is removed rather than kept as a trap.
  A configuration file that still says `"popup"` falls back to `actionbar`, exactly like any
  other value the channel list does not know. `actionbar` and `tip` remain.

### Fixed
- A block container's contents no longer turn into the player's own inventory once its screen
  has been opened. The screen's models map holds the player's inventory models next to the block's,
  and the player's main inventory has exactly as many slots as a chest (27), so matching by slot
  count picked the wrong model - and the snapshot it wrote then kept the panel on it, because a
  snapshot is only dropped once it agrees with the world copy again. Models whose category is the
  player inventory are never candidates now, and a model that wraps the block's own container (which
  is how a brewing stand's three models are found) is preferred outright.
- A message-only text packet - what the `tip` channel sends - now carries a fully filled payload:
  the localize flag, the filtered message and the sender's xuid and platform id, instead of
  nothing but the message body, which the client can reject as a malformed packet.
- The two client hotkeys only fire where they belong. They used to act on any key press, so the
  key that opens the screen also fired while typing in chat, in an anvil or on a sign, and in the
  main menu, where it had no business being. A hotkey now needs the game to be playing - the
  screen that owns the input has to be the HUD one, which also covers every other menu, since the
  screens behind an open one report no focus - and no text box may be selected for typing, which
  the client reports for chat, signs, the anvil and the creative search field alike. While the
  configuration screen itself is up its own keys are left alone too: a rebind capture or an edited
  text field no longer gets its key eaten by the hotkey.
- The status effect line shows **every** effect again, with the right level. The engine's effect list
  carries slots it has no name for, and counting those made the line stop after the first effect and
  report the rest as `+n`; only the entries that produced a name are counted now. The name and the
  level come from the game's own effect table (`potion.*`, so the same wording the game itself uses)
  rather than from the engine's display name, which carries the level on one side and none on the
  other - which is what turned a level II speed into `Speed II II`.
- The status effect line is a **server-side** one now. A client is never told more than the latest
  effect of another creature - measured: the server's list holds every effect in its id slot, a
  client's holds that single latest one - so on a client the line could only show one effect and then
  disappear with it. Rather than extrapolate the rest, the line is left to the side that owns the data.
- A horse's numbers are the ones a player can judge: **speed in blocks per second** (the attribute
  times 42.16) and the **jump height in blocks** (the jump strength through the cubic the game's horse
  jump uses), instead of the raw per-tick engine values - `Speed 7.34 blocks/s · Jump height 3.25 blocks`.
- A villager's trade level is one higher than the raw value: the game counts the tiers from zero, so
  the number now matches the Novice / Apprentice / ... the trade screen shows.
- A zombie villager's conversion time is read from the transformation countdown the game itself runs
  (`TransformationComponent::mDelayTicks`): the per-class field is zero on this build and the
  "converting" flag is not set on every converting villager, so the line never appeared. A client, which
  is never told the countdown, shows the state on its own when it is told at all.
- The copper golem's waxed line reads **yes / no** instead of the on/off of a switch, and it is a
  server-side line: the game declares that property as not client-synced, so a client would only ever
  read the default and say "no" about a golem that is in fact waxed.
- The breeding line appears for a client as well: the counters are server-side state, so the line falls
  back to the "in love" flag the client _is_ told (`Breeding In love`) instead of staying empty because
  the component is missing there.

## [0.3.1] - 2026-10-02
### Fixed
- The README says **LeviLamina 26.51.\*** on both sides now, which is what `tooth.json` declares and
  what the mod is built against; 0.3.0 already required 26.51 in `tooth.json` while its README still
  said 26.40.

## [0.3.0] - 2026-10-01
### Added
- The panel fades in when the info display appears and out when it disappears; when the crosshair
  moves to another block or entity the same panel resizes its box from the size it had. The lines are
  swapped at once and nothing is faded while that happens, so changing the subject never flashes.
  `client.transitionTime` sets the duration in seconds (default 0.1); `0` keeps the previous behaviour
  of appearing, resizing and disappearing instantly. Only the display switching and the box resizing
  animate. The client overlay is the only side that can animate: a server-side channel hands text to
  the vanilla UI.
- A configuration file written before the switches existed keeps the panel it had: while upgrading, a
  file below schema version 5 has its legacy `format` / `entityFormat` templates read once - before
  the config library merges the new defaults in and rewrites the file - and turned into switches
  group by group. The block `format` template feeds the `display.*` switches and the entity
  `entityFormat` template the `entity.*` switches, a placeholder the template used meaning that part
  stays visible and a placeholder it never used meaning that switch is off. An old default `format`
  (`{blockName}`, `{blockType}`, `{x} {y} {z}`, `{extras}`) therefore ends up with `display.name`,
  `display.identifier`, `display.position` and `display.extras` on and the rest of the block group
  off, and the entity template's placeholders map onto `entity.*` the same way - for example
  `{entityName}` -> `entity.name`, `{entityType}` -> `entity.identifier`, `{x} {y} {z}` ->
  `entity.position`, `{health}` / `{maxHealth}` -> `entity.health`, `{extras}` -> `entity.extras`. A
  template that is not in the file at all leaves its group at the defaults rather than switching
  everything off. The migration runs only for a file below version 5 and the result is saved
  immediately. The per-block-type `overrides` have no equivalent and are still dropped, and `{dim}`
  has no switch to feed either.
- Blocks and entities have a panel of their own now: `display.*` (nine switches) describes a block
  and `entity.*` (eight switches) an entity, and `entity.extras` is the entity panel's own switch for
  per-entity extra lines, the counterpart of `display.extras`. The entity panel reuses the same
  `extras.*` adapters (`extras.enabled` gates the block and the entity side alike): container
  entities such as chest / hopper minecarts and boats with chest come from `extras.chest`, paintings
  from `extras.painting` and the worn equipment of players, armor stands and armored mobs from
  `extras.misc`, which reports the four armor slots plus the main and off hand.

### Changed
- Requires **LeviLamina 26.51.\*** (Bedrock 26.51) on both sides: the client variant wants the
  LeviLamina client build (`#client`), the server variant the plain one, and both are built against
  26.51 now, so a 0.2.0 install on 26.40 has to update LeviLamina first.
- Higher `Config` schema version (5) for the separate `display` / `entity` switch groups, the removal
  of the templates and the new `client.transitionTime` option; existing files are merged
  automatically, and a file below version 5 has its old templates migrated into switches first.
- The panel is assembled by the mod from per-part switches instead of the `format` / `entityFormat`
  text templates, and the two targets no longer share one switch set: `display.*` feeds the block
  panel and `entity.*` the entity panel. Each is laid out in the same five lines: the name with the
  facing in muted brackets (`Stone§7(north)`), the type id with its translation key in brackets
  (`§7minecraft:stone(tile.stone.stone)`), the position with the distance behind it, the labelled
  stats (each `§7<label> §f<value>` and separated by one space) and the extras. The block panel's
  stats are the light level and the light the block emits (`Light 12 Emission 15`). A part the target
  does not have (health on an entity type without any, a facing on a block whose
  states we do not understand) is skipped and empty lines are dropped, so a malformed layout (a stray
  placeholder, a dangling separator, a blank line) is no longer possible. The `·` that joined every
  format part is gone (the only one left is inside a container's extras line, between the slot count
  and the total item count) and the dimension is not shown any more.
- The entity panel no longer reports a light level or light emission: both were read from the block
  under the entity, which describes a world position rather than the entity itself, so
  `display.light` / `display.emission` are block-only now and leave an entity panel untouched.
  `display.health` is `entity.health` now, next to the rest of the entity switches, and there is no
  `display.dimension` at all. `/insight set` accepts every `display.*` and `entity.*` name.
- The client info panel is drawn by the game's own UI renderer now (engine font, engine item renderer
  for the subject icon, rounded background, fade and size animation over `client.transitionTime`)
  instead of the ImGui overlay, so its text and icon match the vanilla UI.
- The configuration screen no longer has "Block format" / "Entity format" text boxes. Its left column
  is tabbed instead of one long stack of sections, and the tabs follow the two switch groups:
  **General**; **Block lines**, holding Title line (Name, Facing), Details line (Type id, Translation
  key), Position line (Position, Distance), State line (Light, Emission) and Extras (Extras lines ->
  `display.extras`); **Entity info**, holding Entity info (`entityEnabled`), Title line (Name,
  Facing), Details line (Type id, Translation key), Position line (Position, Distance), State line
  (Health) and Extras (Extras lines -> `entity.extras`); **Extras**, holding Extras (`extras.enabled`
  only - the `display.extras` row moved into the Block lines tab), Extras: every block, Extras:
  containers, Extras: block entities, Extras: redstone and Extras: block states; and **Keys**.
  Preview and Appearance stay in the right column.

### Removed
- `format`, `entityFormat` and the per-block-type format `overrides`
  (`{ "match": ..., "format": ... }`), together with their placeholders (`{blockName}`,
  `{blockType}`, `{blockKey}`, `{x}` `{y}` `{z}`, `{dist}`, `{dim}`, `{direction}`, `{light}`,
  `{emission}`, `{extras}`, `{entityName}`, `{entityType}`, `{health}`, `{maxHealth}`).
  `/insight set` no longer accepts `format` / `entityFormat` and answers `Unknown option: ...` for
  them.

### Fixed
- The subject icon was centred on the whole panel, so a block with many extra lines left it
  floating in the middle of the box, away from the name it belongs to. It sits at the top of its
  cell now, level with the first text line, and it is one and a half text lines tall (measured from
  the same line height the text uses, so it still follows `client.fontSize`) instead of the fixed
  16 units that made it read as a bullet next to the first line.
- `/insight set <option> <value>` rejected every number - `/insight set maxDistance 24` answered
  `Syntax error: Unexpected "24"` - and anything containing a space: the value was declared as a
  `std::string`, which looks like an identifier to the command parser. It is read as raw text now, so
  numbers, colours and free text can be typed the way the README documents them. Values are still
  validated (`Invalid number value: bogus`).

## [0.2.0] - 2026-09-25
### Added
- Per-block extra info with **one switch per adapter** (`extras.*`), so every line can be silenced on
  its own - in the configuration screen or with `/insight set extras.<name> true`: breaking time,
  explosion resistance, container occupancy, chiseled bookshelf, shelf, lectern, decorated pot,
  brewing stand, the furnace family, jukebox record, sign text (front and back), banner patterns,
  item frame, flower pot, painting, piston, redstone level / repeater / comparator, dispenser,
  candle, respawn anchor, and a `misc` group covering doors, trapdoors, fence gates, buttons,
  levers, tripwire hooks, observers, sea pickles, cake, composters, beds, enchanting tables, bee
  nests, beacons, campfires and the crafter (its two states and its disabled slots).
- Cooking timers (furnace / blast furnace / smoker, brewing stand, campfire) keep running while no
  container screen is open: the client keeps the last value the engine sent and advances it
  locally, moves on to the next item by itself and hides the lines once every item it knew about has
  finished. Fuel running out, or a hopper refilling while nobody looks, is the one thing a client
  cannot see, so the estimate may drift until the screen is opened again.
- The client registers a command of its own, `/cliinsight`, with `toggle` / `on` / `off` next to
  `status` / `reload` / `set` / `gui`. The switch flips `client.showOverlay` through the same path
  as the hotkey, so command and key always agree and both persist the change, and the status line
  reports the display state.
- The command description is localized (`Insight's main command`); unlike the messages it is
  translated once, when the command is registered.

### Changed
- Requires **LeviLamina 26.40.\*** (Bedrock 26.40); the sources no longer compile against 26.20.
- The client command is named `/cliinsight` rather than `/insight`, so a client and a server install
  can coexist: joining a world merges the server's command list into the client registry, which
  would otherwise make the two clash.
- Defaults: `client.anchor` is `top_center` (was `bottom_center`) and `client.overlayOnRemote` is
  `on` (was `off`). Defaults only apply to new or incomplete config files.
- Higher `Config` schema version (3), because every extras adapter became a field of its own;
  existing files are merged automatically.
- Furnaces report only the item that is in the fire (`Time left Ns`, `Cook progress N%`), and the
  percentage uses that machine's own cook time - 200 ticks in a furnace, 100 in a blast furnace or
  smoker - instead of assuming a furnace everywhere.

### Removed
- The note block pitch/instrument and the ignite chance extras: neither is reachable through the
  26.40 API, so their switches were removed together with the code instead of showing guessed
  values.

### Fixed
- `/insight` was unknown on a dedicated server even though the mod was enabled: LeviLamina publishes
  its command-registration event while the server instance is still being built, but enables mods
  later, so the command is now registered directly when the mod is enabled (with a log line to
  confirm it).
- The crafter showed only one of its two states - `triggered` is the Java spelling while Bedrock
  calls it `triggered_bit` - and its disabled slots were read from a state that does not exist
  (they live in the block entity).
- Switching one extras adapter off could take unrelated lines down with it: turning banners off also
  silenced the furnace readout, the brewing percentage and the beehive, beacon and campfire lines.
- `/insight gui` no longer exists on the server, where it could only answer that the screen is
  client-side. The subcommand is registered only where there is a screen to open.

## [0.1.0] - 2026-09-18
### Added
- Client configuration screen (Dear ImGui), opened with `/insight gui` or a hotkey and modal while it
  is open: every option in grouped sections with an inline editor (switch, slider, combo box,
  multi-line text, key binding), the appearance settings, a live preview of the panel, a status line
  for the last change and immediate saving.
- Two configurable hotkeys (open the screen, show/hide the info display), registered with
  LeviLamina so the game's own key settings can remap them, and rebindable from the screen itself.
- A language row that lists exactly the languages this mod ships (plus `auto`); fixed choices such as
  the anchor positions are shown as translated labels instead of their raw tokens.

### Changed
- The configuration screen takes its input from Dear ImGui's Win32 backend through a window-procedure
  hook, so clicking, dragging, scrolling and text editing behave like a normal desktop window. While
  it is open the game receives neither mouse nor keyboard messages, and raw mouse input no longer
  turns the camera.
- Cursor handling on the client was rewritten: the pointer is handed over on the window thread while
  the screen is open and given back on close, so it is visible in the screen and hidden again - and
  still locked inside the game window - while playing.
- `client.language` is a stored preference rather than something detected at runtime (default
  `zh_cn`); `auto` follows the client only when it reports a language this mod actually ships, and a
  locale code is always resolved to the real message file (`zh_CN` -> `zh_cn`) before lookup.
- Offsets are applied relative to the anchored edge: a positive value pushes the panel towards the
  middle of the screen, so the whole `0..1` range is usable with every anchor. The panel is also
  never allowed to grow wider than the display (its text wraps instead), which is what previously
  pinned it to the left edge and made the horizontal offset look ineffective.
- The info panel is not drawn while the configuration screen is open: the screen shows the preview
  instead of a duplicate panel behind it.
- Slider rows keep their own value while they are open, so a drag is no longer undone by the value
  being re-read from the configuration every frame.
- Higher `Config` schema version (2) for the two new hotkey options; existing files are merged
  automatically.

### Removed
- The first, engine-event based input path of the configuration screen (cursor mapping, synthetic
  clicks and the software cursor) together with the debug output used while working on it.

### Fixed
- Changing one option could make every further change do nothing (the queued edits were applied after
  the "mod or overlay switched off" shortcut that editing those very options triggers).
- Two edits made between two ticks of the game thread could drop the earlier one; edits are queued
  now.
- Text options could not be edited a second time because the edit buffer was refilled from the
  configuration every frame.
- Integer options (for example the sampling interval) rejected the decimal values the sliders send.
- Long values (block and entity formats) spilled over the neighbouring rows and out of the window;
  they are folded onto one line and truncated now.
- The pointer stayed visible after closing the configuration screen and could leave the game window.
- Enum options showed their raw tokens (`top_left`, `on`) instead of a readable, translated label.

## [0.0.3] - 2026-09-16
### Fixed
- fixed tooth.json

## [0.0.2] - 2026-09-13
### Fixed
- fixed tooth.json

## [0.0.1] - 2026-09-12
First release: the mod template has been turned into the actual mod.

### Added
- Server build (BDS) that samples every online player's look ray on a fixed cadence and sends the
  formatted info to that player only, through a configurable channel
  (`actionbar` / `tip` / `popup` / `jukebox` / `system` / `chat` / `none`).
- Client build (GDK / LeviLamina client) that draws the panel locally with Dear ImGui over a DXGI
  Present hook, because the vanilla UI text path cannot render CJK block names on this GDK build.
- Info content: block name / type / translation key / coordinates / distance / dimension, the
  `{direction}`, `{light}` and `{emission}` placeholders, and per-block extra lines via `{extras}`.
- Extras adapters: container occupancy (chest, trapped chest, barrel, hopper, dropper, dispenser,
  copper chest, shulker box, crafter, chiseled bookshelf, lectern, decorated pot, chest and hopper
  minecart, boat with chest), furnace / blast furnace / smoker and brewing stand slots, redstone
  signal levels, block states (doors, trapdoors, fence gates, buttons, levers, tripwire hooks,
  observers, pistons, crafters, composters, cake, note blocks, sea pickles, enchanting tables) and
  block entities (banners, decorated pots, shelves, item frames, lecterns), plus the equipment of
  every entity that carries some.
- Entity support: an entity is shown when it is closer than the block under the crosshair, with
  `entityEnabled` / `entityFormat` and `{health}` / `{maxHealth}`.
- Commands: `/insight toggle | on | off | status | reload` and `/insight set <option> <value>` with
  option-name completion, value validation and localized feedback.
- Per-player switches are stored in a key-value database (`data/players/`), so they survive server
  restarts.
- Localization through LeviLamina's i18n (`lang/en.json`, `lang/zh_cn.json`) with a locale fallback
  chain; block and item names come from the engine's own localization tables, so resource pack
  translations are honoured.
- Configuration: `config.json` with automatic merging of new options, per-block-type `overrides`,
  and client overlay options (anchor, offsets, font size, background, shadow, text colour, max
  width, hide in GUI, overlay on remote servers, language).
- Project icon (`assets/icon.svg`, `assets/icon.png`) and a user-facing README in English and
  Chinese.

### Changed
- Detection is data-driven: the engine is asked directly (health attribute,
  `Block::isContainerBlock()`, material liquids, block actor types, block states, dimension name)
  instead of the mod keeping lists of block and entity ids.
- Documentation is user-facing; implementation notes live in code comments.

### Removed
- The mod template leftovers (`src/mod/MyMod.*`).

[Unreleased]: https://github.com/neverforward/Insight/compare/v0.5.0...HEAD
[0.5.0]: https://github.com/neverforward/Insight/compare/v0.4.0...v0.5.0
[0.4.0]: https://github.com/neverforward/Insight/compare/v0.3.1...v0.4.0
[0.3.1]: https://github.com/neverforward/Insight/compare/v0.3.0...v0.3.1
[0.3.0]: https://github.com/neverforward/Insight/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/neverforward/Insight/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/neverforward/Insight/compare/v0.0.3...v0.1.0
[0.0.3]: https://github.com/neverforward/Insight/compare/v0.0.2...v0.0.3
[0.0.2]: https://github.com/neverforward/Insight/compare/v0.0.1...v0.0.2
[0.0.1]: https://github.com/neverforward/Insight/releases/tag/v0.0.1
