#pragma once

#include <functional>

#include "mc/world/actor/player/Player.h"

namespace insight {

// Toggles the per-player switch; called with the executing player, returns the
// *new* enabled state. Server-only - pass nullptr on the client.
using PlayerToggleFn = std::function<bool(Player&)>;

// Opens the client-side configuration screen. Client-only - pass nullptr on the
// server build (the command then reports that the screen is client-side).
using OpenConfigUiFn = std::function<void()>;

// Registers the "/insight" command tree for one side:
//   /insight toggle | on | off   (only when `toggleFn` is given - server side)
//   /insight status
//   /insight gui                 (client side: opens the configuration screen)
//   /insight reload              (OP)
//   /insight set <option> <value> (OP; edits + persists the config file)
void registerInsightCommand(bool isClientSide, PlayerToggleFn toggleFn = nullptr, OpenConfigUiFn openUi = nullptr);

} // namespace insight
