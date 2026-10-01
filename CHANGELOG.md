# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

<!-- The release workflow reads the section matching the released tag from this
     file (ffurrer2/extract-release-notes), so the version heading below has to
     match the tag, e.g. tag `v1.0.0` -> `## [1.0.0] - YYYY-MM-DD`. -->

## [Unreleased]

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

[Unreleased]: https://github.com/neverforward/Insight/compare/v0.2.0...HEAD
[0.2.0]: https://github.com/neverforward/Insight/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/neverforward/Insight/compare/v0.0.3...v0.1.0
[0.0.3]: https://github.com/neverforward/Insight/compare/v0.0.2...v0.0.3
[0.0.2]: https://github.com/neverforward/Insight/compare/v0.0.1...v0.0.2
[0.0.1]: https://github.com/neverforward/Insight/releases/tag/v0.0.1
