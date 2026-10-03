#pragma once

#include "mc/world/actor/player/Player.h"

namespace insight::gui {

// The server's configuration menu: `/insight gui` opens a group picker and every group is
// one form whose values are written back through Insight::applyConfigEdit() - so the same
// validation, range checks and persistence the `/insight set` command uses apply here too.
//
// A server has no screen of its own: a form is pushed to one player, which is why this
// takes the player who asked. The client-only options (showOverlay, the anchor, the font,
// the key bindings, ...) are deliberately absent - they do nothing on a server, and every
// other option the command accepts is covered (see .cache/check_gui_options.py).
//
// Must be called on the server thread, which is where command execution runs.
void showMenu(Player& player);

} // namespace insight::gui
