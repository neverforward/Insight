#pragma once

#include <functional>
#include <string>
#include <vector>

namespace insight {

// Always-on HUD overlay rendered through Dear ImGui on the DXGI Present
// hook. The vanilla UI text pipeline (MinecraftUIRenderContext::drawText)
// cannot render CJK names on this GDK build (only isolated glyphs ever
// showed), so the client panel is drawn with ImGui's own CJK-capable font
// (Microsoft YaHei) instead. No ImGui input backend is installed: this is a
// pure display overlay, so nothing is subclassed and no input is swallowed.
class ImGuiOverlay {
public:
    // One colored text run of a line (already split on legacy color codes).
    struct Run {
        std::string text;
        float       r = 1.0f;
        float       g = 1.0f;
        float       b = 1.0f;
    };

    // A full frame payload, snapshotted by the caller on the game thread and
    // rendered by the overlay on the render thread (guard with the mutex).
    struct Content {
        bool                          visible = false;
        std::vector<std::vector<Run>> lines; // one entry per '\n' line
        std::string                   anchor          = "bottom_center";
        float                         offsetX         = 0.0f;
        float                         offsetY         = 0.0f; // positive = up
        float                         fontSize        = 1.0f;
        bool                          background      = true;
        float                         backgroundAlpha = 0.45f;
        bool                          shadow          = true;
        // Max panel width as a fraction of the screen width (0 = no limit).
        // Longer lines are wrapped at character boundaries.
        float maxWidth = 0.0f;
    };

    ImGuiOverlay() = default;
    ~ImGuiOverlay();

    ImGuiOverlay(ImGuiOverlay const&)            = delete;
    ImGuiOverlay& operator=(ImGuiOverlay const&) = delete;

    /// Install the DXGI hooks (idempotent). Safe to call before the game's
    /// swap chain exists; ImGui is bootstrapped lazily on the first Present.
    /// Returns true when the hooks are in place.
    bool install();

    /// Remove the hooks and destroy the ImGui context/backend.
    void shutdown();

    [[nodiscard]] bool installed() const;

    /// Push the content to show from the next Present onward.
    void setContent(Content content);

    /// Optional extra ImGui draw callback (e.g. the configuration screen). It
    /// runs on the render thread inside the ImGui frame, right after the HUD
    /// content, so it can use ImGui freely.
    void setWindowDrawer(std::function<void()> drawer);

    /// Draws the *current* HUD content (same runs, colours, shadow and font as
    /// the on-screen panel) into the given rectangle - the configuration
    /// screen's preview uses this so it shows the real display instead of a
    /// re-implementation. `scale` multiplies the HUD font size.
    static void drawContentPreview(
        float              x,
        float              y,
        float              width,
        float              height,
        float              scale,
        std::string const& anchor,
        float              offsetX,
        float              offsetY
    );

    /// While this is true the game window procedure swallows mouse/key messages,
    /// so the configuration screen owns the input (modal).
    static void setInputCaptured(bool captured);
};

} // namespace insight
