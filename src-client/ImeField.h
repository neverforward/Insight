#pragma once

#include <cstddef>

struct ImVec2;

namespace insight::ImeField {

// In-place text input for the configuration screen.
//
// ImGui gets no IME from this client: the game window never produces a composition, so an InputText
// can only take ASCII and the candidate window never appears. A text field is therefore edited by a
// Win32 EDIT control of our own - the system IME works on that one normally - placed exactly over the
// ImGui widget, with every change copied into the caller's UTF-8 buffer.
//
// `buffer` must outlive the edit (until active() is false): it is written from the input window's
// own thread. `itemMin`/`itemMax` are ImGui coordinates (the game window's client space).

/// The game's top-level window. Called every frame, cheap.
void setOwnerWindow(void* window);

/// Starts editing `buffer` with a native control placed over the given rectangle.
bool begin(char const* what, char* buffer, size_t size, ImVec2 const& itemMin, ImVec2 const& itemMax);

/// Ends the edit (what was typed is already in the buffer).
void end();

/// True while an edit is up.
[[nodiscard]] bool active();

/// True while the edit is the one for `buffer`.
[[nodiscard]] bool belongsTo(char const* buffer);

/// Follows the widget with the native control.
void follow(ImVec2 const& itemMin, ImVec2 const& itemMax);

/// What the native control holds right now (UTF-8).
[[nodiscard]] char const* value();

/// Call once per drawn frame: closes the edit when the screen stops showing it.
void tick();

} // namespace insight::ImeField
