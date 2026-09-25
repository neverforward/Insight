#pragma once

#include <functional>

#include "mc/world/actor/player/Player.h"

namespace insight {

// Toggles the per-player switch; called with the executing player, returns the
// *new* enabled state. Server-only - pass nullptr on the client.
using PlayerToggleFn = std::function<bool(Player&)>;

// Opens the configuration screen. Pass nullptr where there is no screen: the
// `gui` subcommand is then not registered at all, so a side without a screen
// does not advertise it.
using OpenConfigUiFn = std::function<void()>;

// Registers the command tree for one side. The name differs per side so a client
// and a server install can coexist: `/insight` on the server, `/cliinsight` on the
// client (LeviLamina uses the same `cli` prefix for its own command). The
// subcommands are identical:
//   /insight toggle | on | off   (only when `toggleFn` is given: the server's
//                                 per-player switch, the client's overlay switch)
//   /insight status
//   /insight gui                 (only when `openUi` is given: client for now)
//   /insight reload              (OP)
//   /insight set <option> <value> (OP; edits + persists the config file)
void registerInsightCommand(bool isClientSide, PlayerToggleFn toggleFn = nullptr, OpenConfigUiFn openUi = nullptr);

} // namespace insight
