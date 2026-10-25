![Insight](https://socialify.git.ci/neverforward/Insight/image?custom_description=A+lightweight+info+display+mod+for+Bedrock+Edition+that+shows+you+exactly+what+you+are+looking+at.&description=1&font=Inter&forks=1&issues=1&logo=https%3A%2F%2Fraw.githubusercontent.com%2Fneverforward%2FInsight%2Frefs%2Fheads%2Fmain%2Fassets%2Ficon.svg&name=1&owner=1&pattern=Plus&pulls=1&stargazers=1&theme=Auto)

![Static Badge](https://img.shields.io/badge/English-inactive?style=for-the-badge)
[![Static Badge](https://img.shields.io/badge/简体中文-informational?style=for-the-badge)](README.zh.md)
![GitHub License](https://img.shields.io/github/license/neverforward/Insight?style=for-the-badge)
![GitHub Tag](https://img.shields.io/github/v/tag/neverforward/Insight?style=for-the-badge)
![GitHub Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/neverforward/Insight/build.yml?style=for-the-badge)
![GitHub commit activity](https://img.shields.io/github/commit-activity/y/neverforward/Insight?style=for-the-badge)

**Insight** is an info display mod for Minecraft Bedrock Edition (LeviLamina 26.51.\*)

| Shulker box | Furnace | Horse | Armour stand | ... |
| --- | --- | --- | --- | --- |
| ![Shulker box](assets/shulker_box.png) | ![Furnace](assets/furnace.png) | ![Horse](assets/horse.png) | ![Armour stand](assets/armor_stand.png) | and more to find |

On the client the panel is drawn by the mod itself; on a server it is shown in the action bar.

Every option can be changed in game.

![Configuration screen](assets/config_page.png)


- **Blocks and block entities**: name, type ID, translation key, coordinates, distance, facing, light level, plus the extra lines
- **Entities**: name, type, coordinates, hit points and armour (bar / hearts / numbers), owner, status effects, breeding and conversion timers, pet armour, horse and llama numbers, armour stand poses.
- **Containers**: chests, barrels, shulker boxes, hoppers, crafters, decorated pots, chest minecarts and furnaces all show the items inside. Like items can be merged into one entry
- **Furnace**: time left and cook progress

## Requirements

- Windows 10 or newer
- Install [LeviLauncher](https://github.com/LiteLDev/LeviLauncher)

## Installation

```bash
lip install github.com/neverforward/Insight          # server (BDS)
lip install github.com/neverforward/Insight#client   # client (GDK / LeviLamina client)
```

The first start writes the configuration file - `plugins/Insight/config/config.json` on a server,
`mods/Insight/config/config.json` on a client - with every default value.

## Usage

- `/insight gui` opens the configuration (on the client, `/cliinsight gui` or the hotkey, default <kbd>I</kbd>)
- `/insight set <option> <value>` changes a setting
- `/insight reload` re-reads the configuration (operator only)
- `/insight toggle` shows or hides the panel (on the client the same command is `/cliinsight toggle`; the hotkey, default <kbd>K</kbd>, toggles it too)

## Documentation

| Document | Contents |
| --- | --- |
| [docs/commands.md](docs/commands.md) | the commands |
| [docs/configuration.md](docs/configuration.md) | the configuration |
| [docs/building.md](docs/building.md) | building both sides, and the project layout |
| [CONTRIBUTING.md](CONTRIBUTING.md) | how to contribute |
| [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) | the rules for taking part |
| [CHANGELOG.md](CHANGELOG.md) | what changed in each release |

## License

MIT © neverforward
