#pragma once

#include <chrono>
#include <string>
#include <vector>

#include "ll/api/event/EventBus.h"
#include "ll/api/event/render/UIRenderEvent.h"

#include "mc/world/level/BlockPos.h"

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

    /// Convert the current mText/mVisible + client config into an overlay
    /// content payload and hand it to the ImGui overlay (render thread).
    void pushOverlay();

    std::vector<ll::event::ListenerPtr> mListeners;

    ImGuiOverlay mOverlay;

    std::chrono::steady_clock::time_point mLastSample{};
    std::string                           mText;
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
