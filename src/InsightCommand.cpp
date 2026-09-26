#include "InsightCommand.h"

#include <cstdint>
#include <string>
#include <vector>

#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/CommandRegistrar.h"
#include "ll/api/command/EnumName.h"
#include "ll/api/command/SoftEnum.h"
#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandRawText.h"
#include "mc/server/commands/CommandOutput.h"

#include "Config.h"
#include "I18n.h"
#include "Insight.h"
#include "Util.h"
#include "mc/server/commands/CommandPermissionLevel.h"

namespace insight {

// The option name of `/insight set <option> <value>` is a command enum: its
// entries are the option names themselves, so the game offers `extras.enabled`,
// `format`, ... as completion. It is a *soft* enum on purpose - the parser then
// accepts any string, so a typo still reaches applyConfigEdit() and produces a
// real error message instead of being rejected as an unknown enum value.
// The name is deliberately distinctive: command enums share one registry-wide
// namespace, so a generic name could clash with another mod.
enum class InsightConfigOption : std::uint64_t {};

using InsightOptionParam = ll::command::SoftEnum<InsightConfigOption>;

// Command parameter struct for `/insight set <option> <value>`. Must have
// external linkage for the command system's reflection.
//
// The value is a CommandRawText, not a std::string: a std::string parameter parses
// as Bedrock's `Id` non-terminal, which only accepts identifier-like tokens, so
// every number ("0.5", "24") and anything containing a space was rejected with a
// syntax error before the command even ran. RawText takes the rest of the line,
// which is what a value typed by a player is.
struct InsightSetParam {
    InsightOptionParam option;
    CommandRawText     value;
};

// Fallback parameter used only when this registry cannot hold soft enums; the
// option stays a plain string then (no completion, same behaviour).
struct InsightSetRawParam {
    std::string    option;
    CommandRawText value;
};

// The canonical option names, identical to the names accepted by
// applyConfigEdit() and documented in README.md.
std::vector<std::string> const& optionNames() {
    static std::vector<std::string> const names{
        "enabled",
        "enabledByDefault",
        "maxDistance",
        "intervalTicks",
        "passThroughLiquids",
        "showEmpty",
        "emptyText",
        "format",
        "channel",
        "showOverlay",
        "anchor",
        "offsetX",
        "offsetY",
        "fontSize",
        "maxWidth",
        "transitionTime",
        "background",
        "backgroundAlpha",
        "shadow",
        "textColor",
        "language",
        "hideOverlayInGui",
        "overlayOnRemote",
        "keyOpenConfig",
        "keyToggleShow",
        "entityEnabled",
        "entityFormat",
        "extras.enabled",
        "extras.hardness",
        "extras.blastResistance",
        "extras.chest",
        "extras.bookshelf",
        "extras.shelf",
        "extras.lectern",
        "extras.pot",
        "extras.brewing",
        "extras.furnace",
        "extras.jukebox",
        "extras.sign",
        "extras.banner",
        "extras.itemFrame",
        "extras.flowerPot",
        "extras.painting",
        "extras.piston",
        "extras.redstone",
        "extras.repeater",
        "extras.comparator",
        "extras.dispenser",
        "extras.candle",
        "extras.respawnAnchor",
        "extras.misc",
    };
    return names;
}

namespace {

Player* playerFromOrigin(CommandOrigin const& origin) {
    auto* entity = origin.getEntity();
    if (!entity || !entity->isPlayer()) {
        return nullptr;
    }
    return static_cast<Player*>(entity);
}

std::string statusText(std::string const& localeCode) {
    auto const& cfg  = Insight::cfg();
    std::string s    = tr(localeCode, "Insight") + " " + tr(localeCode, cfg.enabled ? "on" : "off");
    s               += " | " + tr(localeCode, "interval") + " " + std::to_string(cfg.intervalTicks) + "t";
    s               += " | " + tr(localeCode, "dist") + " " + std::to_string((int)cfg.maxDistance);
#ifdef INSIGHT_TARGET_SERVER
    s += " | " + tr(localeCode, "channel") + " " + cfg.server.channel;
#else
    s += " | " + tr(localeCode, "anchor") + " " + cfg.client.anchor;
    s += " | " + tr(localeCode, "display") + " " + tr(localeCode, cfg.client.showOverlay ? "on" : "off");
    s += " | " + tr(localeCode, "lang") + " "
       + (cfg.client.language.empty() ? tr(localeCode, "auto") : cfg.client.language);
#endif
    if (cfg.extras.enabled) {
        s += " | extras " + tr(localeCode, "on");
    }
    return s;
}

bool mayManage(CommandOrigin const& origin) {
    return origin.getPermissionsLevel() >= CommandPermissionLevel::GameDirectors;
}

} // namespace

void registerInsightCommand(bool isClientSide, PlayerToggleFn toggleFn, OpenConfigUiFn openUi) {
    auto& registrar = ll::command::CommandRegistrar::getInstance(isClientSide);

    // Register the option list before declaring the overload so the game can
    // complete it. Registries that cannot hold soft enums fall back to a plain
    // string option below.
    auto const enumName = std::string(ll::command::enum_name_v<InsightConfigOption>);
    bool const softEnum = registrar.hasSoftEnum(enumName) || registrar.tryRegisterSoftEnum(enumName, optionNames());

    // The two sides register differently named commands, the way LeviLamina's own
    // /levilamina and /clilevilamina do. Joining a world merges the server's
    // command list into the client registry and then lets client mods register on
    // top of it, so an unprefixed client-side /insight would fight with a
    // server-side /insight whenever both installs are present; the `cli` prefix
    // keeps them apart and both stay usable. The tree below is identical.
    auto const commandName = isClientSide ? "cliinsight" : "insight";

    // The description is baked into the command registry when it is registered,
    // so - unlike every message the command prints - it cannot follow the locale
    // of whoever asks: it is translated once, into the language this install is
    // configured for.
    auto const description =
        tr(resolveLanguageCode(Insight::cfg().client.language), "Insight's main command");

    auto& cmd = registrar.getOrCreateCommand(commandName, description, CommandPermissionLevel::Any);

    if (toggleFn) {
        // --- per-player switches (server only) --------------------------
        cmd.overload().text("toggle").execute([toggleFn](CommandOrigin const& origin, CommandOutput& output) {
            auto* player = playerFromOrigin(origin);
            if (!player) {
                output.error(tr(origin.getLocaleCode(), "This command can only be run by a player."));
                return;
            }
            bool now = toggleFn(*player);
            output.success(tr(origin.getLocaleCode(), now ? "Insight enabled for you." : "Insight disabled for you."));
        });
        cmd.overload().text("on").execute([toggleFn](CommandOrigin const& origin, CommandOutput& output) {
            auto* player = playerFromOrigin(origin);
            if (!player) {
                output.error(tr(origin.getLocaleCode(), "This command can only be run by a player."));
                return;
            }
            toggleFn(*player);
            output.success(tr(origin.getLocaleCode(), "Insight enabled for you."));
        });
        cmd.overload().text("off").execute([toggleFn](CommandOrigin const& origin, CommandOutput& output) {
            auto* player = playerFromOrigin(origin);
            if (!player) {
                output.error(tr(origin.getLocaleCode(), "This command can only be run by a player."));
                return;
            }
            toggleFn(*player);
            output.success(tr(origin.getLocaleCode(), "Insight disabled for you."));
        });
    }

    // --- shared ---------------------------------------------------------
    cmd.overload().text("status").execute([](CommandOrigin const& origin, CommandOutput& output) {
        output.success(statusText(origin.getLocaleCode()));
    });

    // --- gui: the configuration screen -------------------------------------
    // Registered only where there is a screen to open. The server has none yet,
    // so it does not advertise a subcommand that could only answer "not here";
    // it will pass `openUi` once it has a screen of its own.
    if (openUi) {
        cmd.overload().text("gui").execute([openUi](CommandOrigin const& origin, CommandOutput& output) {
            openUi();
            output.success(tr(origin.getLocaleCode(), "Insight configuration screen opened."));
        });
    }

    cmd.overload().text("reload").execute([isClientSide](CommandOrigin const& origin, CommandOutput& output) {
        if (!isClientSide && !mayManage(origin)) {
            output.error(tr(origin.getLocaleCode(), "You do not have permission to reload Insight."));
            return;
        }
        Insight::reloadConfigFromDisk();
        output.success(tr(origin.getLocaleCode(), "Insight configuration reloaded."));
    });

    // --- /insight set <option> <value> ----------------------------------
    auto applySet = [isClientSide](
                        CommandOrigin const& origin,
                        CommandOutput&       output,
                        std::string const&   option,
                        std::string          value
                    ) {
        // raw text keeps whatever spacing the player typed around the value
        util::trimInPlace(value);
        auto const localeCode = origin.getLocaleCode();
        if (!isClientSide && !mayManage(origin)) {
            output.error(tr(localeCode, "You do not have permission to change Insight."));
            return;
        }
        if (auto result = Insight::applyConfigEdit(option, value, localeCode); result.ok) {
            output.success(result.message);
        } else {
            output.error(result.message);
        }
    };

    if (softEnum) {
        cmd.overload<InsightSetParam>().text("set").required("option").required("value").execute(
            [applySet](CommandOrigin const& origin, CommandOutput& output, InsightSetParam const& param) {
                applySet(origin, output, param.option, param.value.mText);
            }
        );
        // Registering the overload may have added the enum's own (empty) value
        // list; make sure exactly the option names remain completable.
        if (registrar.hasSoftEnum(enumName)) {
            registrar.setSoftEnumValues(enumName, optionNames());
        }
    } else {
        cmd.overload<InsightSetRawParam>().text("set").required("option").required("value").execute(
            [applySet](CommandOrigin const& origin, CommandOutput& output, InsightSetRawParam const& param) {
                applySet(origin, output, param.option, param.value.mText);
            }
        );
    }

    if (!softEnum) {
        // Quiet on success: this only matters when the option names cannot be
        // offered as completion on this side.
        Insight::getInstance().getSelf().getLogger().info(
            "insight set: option enum '{}' is not available as a soft enum here, using a plain text option",
            enumName
        );
    }
}

} // namespace insight
