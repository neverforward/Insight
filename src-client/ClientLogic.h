#pragma once

#include <chrono>
#include <string>
#include <vector>

#include "ll/api/event/EventBus.h"
#include "ll/api/event/render/UIRenderEvent.h"

#include "mc/world/level/BlockPos.h"
#include "mc/world/item/ItemStack.h"

#include "ImGuiOverlay.h"
#include "PlatformLogic.h"

namespace insight {

// Client (GDK / LeviLamina client) implementation: every render frame the
// local player's look ray is sampled (throttled to the configured cadence)
// against the *client-side* world copy and the formatted text is drawn as an
// overlay through the UI render context. Nothing is ever sent to the server.
class ClientLogic final : public PlatformLogic {
public:
    ClientLogic() = default;
    ~ClientLogic() override;

    bool enable() override;
    void disable() override;

private:
    void onRender(ll::event::render::BeforeUIRenderEvent& event);

    /// Draws the current subject's icon with the game's own item renderer, into the slot the
    /// overlay published, and reports the rectangle it used back. Runs after the screen's UI
    /// so the icon lands on top of it; the overlay leaves that slot empty for exactly this.
    void onAfterRender(ll::event::render::AfterUIRenderEvent& event);

    /// Draws the whole panel through Minecraft's own UI context: rectangles for the box, the
    /// game's item renderer for the icon and the engine font for the text. Everything shares
    /// the UI's coordinate system, so the icon sits in its cell by construction - the ImGui
    /// overlay draws in pixels on the Present hook, a space the UI pass never sees.
    void drawPanelNative(ll::event::render::AfterUIRenderEvent& event);

    /// Convert the current mText/mVisible + client config into an overlay
    /// content payload and hand it to the ImGui overlay (render thread).
    void pushOverlay();

    std::vector<ll::event::ListenerPtr> mListeners;

    ImGuiOverlay mOverlay;

    // The current subject as an item stack: the only form the game's item renderer accepts,
    // and what onAfterRender hands it. Null for subjects it cannot draw (entities), which
    // simply get no icon.
    ItemStack mIconStack;
    // Subject the stack above was built for. The stack must be reused for as long as the
    // subject lives: a freshly built one makes the engine restart the model/pickup animation,
    // which shows up as a 3D block icon that keeps popping.
    std::string mIconKey;

    // The content the panel actually draws. It lags mText by one step: the sampler clears
    // mText the moment the subject is lost, and drawing nothing there would blink the panel
    // away instead of fading it out.
    std::string mShownText;
    // Fade of the native panel, driven by client.transitionTime, so the display still fades
    // in and out the way the ImGui one did.
    float mNativeFade = 0.0f;
    // Timestamp of the last fade step. The UI render event fires once per screen in the stack,
    // so the fade has to advance by elapsed time instead of once per call.
    std::chrono::steady_clock::time_point mLastPanelTick{};
    // Panel size. mBoxWantW/mBoxWantH is what the current subject asks for: frozen per subject,
    // quantised and grow-only, so a ticking digit cannot jitter the box. mBoxW/mBoxH is what is
    // actually drawn - it eases towards the target, and that easing is the panel's own
    // size-change animation (on a target switch and on content growth alike).
    std::string mBoxKey;
    float       mBoxWantW = 0.0f;
    float       mBoxWantH = 0.0f;
    float       mBoxW     = 0.0f;
    float       mBoxH     = 0.0f;

    std::chrono::steady_clock::time_point mLastSample{};
    std::string                           mText;
    // Changes only when the crosshair moves to another subject; the overlay uses it
    // to crossfade, while a refresh of the same subject's text must not animate.
    std::string                           mTargetKey;
    bool                                  mVisible = false;

    // Refresh triggers: while a GUI is open the panel is hidden and no
    // sampling happens, so the first gameplay frame after closing it must
    // re-sample immediately (container contents / block states changed while
    // the GUI was open). Looking at a different block also refreshes at once.
    std::chrono::steady_clock::time_point mLastInstallAttempt{};
    bool                                  mRefreshRequested = false;
    bool                                  mHadHit           = false;
    BlockPos                              mLastHitPos{};
    std::string                           mLastHitType;

    // Number of consecutive hud_screen passes since the last non-hud (menu)
    // screen was seen. Used to debounce the hide-overlay-in-gui decision:
    // a single stray menu event must not tear the overlay down. Starts
    // "closed" (>= threshold) so the overlay draws from the first frame.
    int mHudStreak = 3;
};

} // namespace insight
