# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

<!-- The release workflow reads the section matching the released tag from this
     file (ffurrer2/extract-release-notes), so the version heading below has to
     match the tag, e.g. tag `v1.0.0` -> `## [1.0.0] - YYYY-MM-DD`. -->

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

[0.0.1]: https://github.com/neverforward/Insight/releases/tag/v0.0.1
