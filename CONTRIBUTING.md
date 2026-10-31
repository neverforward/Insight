# Contributing to Insight

Thanks for taking the time to help. Insight is a small mod with a long list of details, so the most
useful contributions are usually **a precise bug report** or **a small, well-tested change** - not a
large rewrite.

Everyone taking part is expected to follow the [Code of Conduct](CODE_OF_CONDUCT.md).

## Ways to help

- **Report a bug** - see [Reporting a bug](#reporting-a-bug) below.
- **Translate** - the panel and command messages live in `lang/`. Copy `lang/en.json` to
  `lang/<locale>.json`, translate the values (never the keys), and send it. A missing key falls back to
  English, so a partial translation is fine.
- **Improve the docs** - `README.md` and `README.zh.md` are kept in step with each other; fix both.
- **Send a change** - see [Making a change](#making-a-change).

## Requirements

- Windows with Visual Studio 2022+ (the MSVC toolchain components), `clang-cl` and
  [xmake](https://xmake.io/)
- Minecraft Bedrock Edition **26.51.\*** (Windows GDK) and/or BDS **26.51.\*** for in-game testing,
  with LeviLamina **26.51.\***
- [`lip`](https://github.com/LiteLDev/lip) makes installing the result into a test environment easy

## Building

The repository holds one target that is built twice; the side is chosen at configure time:

```bash
# server (BDS)
xmake f -y -p windows -a x64 -m release --target_type=server
xmake

# client (GDK / LeviLamina client)
xmake f -y -p windows -a x64 -m release --target_type=client
xmake
```

The artifacts are written to `bin/Insight/`.

**Both targets have to build with no warnings at all.** The project compiles with `/W4` (plus a short
list of disabled warnings that come from the engine headers), and a warning is treated as a defect, not
as noise. If a new warning appears, fix the code rather than adding a suppression.
### Tests

Tests are written with **gtest**, in the layout the upstream project uses: `src-test/common` for the
tests that do not need a side, `src-test/server` and `src-test/client` for the ones that do. They live
in their own binary, so a test run never needs a game:

```bash
xmake f -y -p windows -a x64 -m debug --tests=y --target_type=server
xmake build test
xmake run test
```

A new feature needs a test when some of its logic is reachable without the engine. Keep that part (a
lookup table, a formatting rule, a decision) in a header that includes no engine headers - the way
`src/NoteNames.h` does - and test that. Configure release again, without `--tests=y`, before building
the mod itself.


## Making a change

### What a change has to keep true

- **Both targets build, warning-free.** Shared code lives in `src/`, so a change there is compiled for
  the server plugin *and* the client mod. Platform-specific code is guarded with
  `INSIGHT_TARGET_CLIENT` / `INSIGHT_TARGET_SERVER`.
- **A configuration option is added or removed in five places.** `Config.h` (the field and its
  comment), `Insight::applyConfigEdit` (validation and ranges), the soft-enum list in
  `InsightCommand.cpp` (autocompletion and `set`), the client screen (`src-client/ConfigUi.cpp`) and the
  server menu (`src-server/ConfigGui.cpp`), plus `lang/en.json` and `lang/zh_cn.json` for its label.
  Adding or removing a field also means **bumping `Config::version`** - an existing file is merged
  automatically, but the version is what tells the mod that the schema changed.
- **Client-only options stay out of the server menu.** The menu only offers options that mean
  something on a server; the client-only ones (overlay, appearance, key bindings, container icons) are
  deliberately absent.
- **Data the client is not sent is not guessed.** The panel reads a server's own world, and on a client
  only the networked copy - which is partial (a machine's cooking counter, a copper golem's waxed flag,
  an entity's full effect list never arrive). Lines built from such data are gated: either the build has
  the data (`kOwnWorldIsComplete`) or the caller says it does (`completeData`). A new line that a client
  cannot know must follow the same rule instead of showing a default that looks like an answer.
- **A server channel carries text and nothing else.** Anything that needs pixels (item icons, hearts,
  bars) is client-only; the server keeps a plain text line for it.
- **The docs move with the code.** `CHANGELOG.md` (under `## [Unreleased]`, with the right heading -
  Added / Changed / Fixed / Removed), `README.md` **and** `README.zh.md`. A user-visible change without
  its changelog entry will be asked for in review.

### Style

- C++20, clang-cl. Match the file you are editing: four spaces, `auto` where it reads better,
  `const`/`[[nodiscard]]` where they say something, one idea per function.
- Comments explain **why**, not what. "The engine only sends a client the slots that changed" is a
  useful comment; "loop over the slots" is not. Write them in English like the rest of the source.
- Prefer engine data over hard-coded lists: block actors, container-ness, effects and item names are
  all readable from the engine, and a list of ids goes stale with the next game update.
- No new third-party dependency without discussing it in an issue first - both targets link the client
  binary or the dedicated server, and every dependency has to build in both.

### Commit messages

[Conventional Commits](https://www.conventionalcommits.org/), in **English**, with an imperative
subject:

```
feat(client): draw a container's contents as item icons
fix: report a furnace's time left and cook progress again
docs: explain the local save reader in both READMEs
```

- Subject at most ~72 characters, no trailing period.
- Body: what changed and **why it had to change** - the engine behaviour that made it necessary is the
  useful part. Wrap at ~100 characters.
- One logical change per commit. A behaviour change and a whitespace-only reformat do not belong in the
  same commit.

### Pull requests

1. Fork, branch off `main`, and keep the branch focused.
2. Build both targets and test the change in game: a client change in a local world (and, if it touches
   the panel, in a remote session too), a server change on a dev server with at least one player.
3. Run the in-game checks that apply: `/insight gui` on both sides, `/insight set` with an invalid value
   (the range check must reject it), reloading the configuration, and the container screen behaviour if
   the change touches containers.
4. Open the pull request against `main` and describe:
   - what the change does and why,
   - which side(s) and which Minecraft/LeviLamina versions you tested on,
   - what you looked at in game to convince yourself it works.

Small pull requests are reviewed much faster than large ones; if a change is going to be big, open an
issue first and describe the plan.

## Reporting a bug

Please include:

- the **side** (client / server / both), the **mod version** (or commit) and the **LeviLamina /
  Bedrock version**;
- what you did, what you expected, and what happened instead;
- the relevant part of the log (`logs/latest.log` for the client, the server console log for BDS) - set
  the log level to debug if the problem only shows there;
- a screenshot of the panel when it is about what is drawn, and the block or entity you were looking at.

A report that names the exact block, entity or option involved is often enough to reproduce the problem
without a follow-up round trip.

## Licence

By contributing you agree that your contribution is licensed under the project's [MIT licence](LICENSE).
