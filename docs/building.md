# Building

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

The artifacts are written to `bin/Insight/`. That whole folder is what you place as described in the
installation section of the [README](../README.md).

# Project layout

| Path | What lives there |
| --- | --- |
| `src/` | the mod itself: configuration, the command, formatting, the extras builders, i18n. Compiled into **both** targets |
| `src-server/` | the BDS side: the sampling loop, the text channel, the `/insight gui` form |
| `src-client/` | the GDK client side: the overlay, the panel drawing, item icons, the configuration screen, the reader for a local save |
| `lang/` | the message catalogues (`en.json`, `zh_cn.json`) |
| `assets/` | the icon and the screenshots the README uses |
| `xmake.lua` | one target, two builds (`--target_type=server` / `client`) |

Shared code is in `src/`; anything that needs a screen or a specific side is behind
`INSIGHT_TARGET_CLIENT` / `INSIGHT_TARGET_SERVER`, so a single `src/` tree produces both the plugin
and the client mod.

Building for a contribution, the checks a change has to pass and the commit conventions are in
[CONTRIBUTING.md](../CONTRIBUTING.md).
