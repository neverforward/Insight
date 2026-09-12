#pragma once

#include <functional>

#include "mc/world/actor/player/Player.h"

namespace insight {

// Toggles the per-player switch; called with the executing player, returns the
// *new* enabled state. Server-only - pass nullptr on the client.
using PlayerToggleFn = std::function<bool(Player&)>;

// Registers the "/insight" command tree for one side:
//   /insight toggle | on | off   (only when `toggleFn` is given - server side)
//   /insight status
//   /insight reload              (OP)
//   /insight set <option> <value> (OP; edits + persists the config file)
void registerInsightCommand(bool isClientSide, PlayerToggleFn toggleFn = nullptr);

} // namespace insight
