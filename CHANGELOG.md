# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

<!-- The release workflow reads the section matching the released tag from this
     file (ffurrer2/extract-release-notes), so the version heading below has to
     match the tag, e.g. tag `v1.0.0` -> `## [1.0.0] - YYYY-MM-DD`. -->

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

[0.2.0]: https://github.com/neverforward/Insight/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/neverforward/Insight/compare/v0.0.3...v0.1.0
[0.0.3]: https://github.com/neverforward/Insight/compare/v0.0.2...v0.0.3
[0.0.2]: https://github.com/neverforward/Insight/compare/v0.0.1...v0.0.2
[0.0.1]: https://github.com/neverforward/Insight/releases/tag/v0.0.1
