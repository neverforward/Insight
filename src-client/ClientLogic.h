#pragma once

#include <chrono>
#include <string>
#include <vector>

#include "ll/api/event/EventBus.h"
#include "ll/api/event/render/UIRenderEvent.h"

#include "mc/world/level/BlockPos.h"
#include "mc/world/item/ItemStack.h"

#include "Extras.h"
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

    // The container under the crosshair, as its adapter reported it, and the item stacks
    // the panel draws from it. The stacks are cached against the list they were built
    // from: a freshly built stack counts as "just picked up" and makes the item renderer
    // replay its pickup squash, which is the popping the block icon had as well.
    ContainerContents          mContainer;
    std::vector<ItemStack>     mItemIcons;
    std::vector<ContainerItem> mItemIconSource;

    // The content the panel actually draws. It lags mText by one step: the sampler clears
    // mText the moment the subject is lost, and drawing nothing there would blink the panel
    // away instead of fading it out.
    std::string mShownText;

    // The sprite rows the panel draws (the entity's hit points and armour, see
    // EntityOptions::hearts): which line of the text each one replaces, and the values
    // they show. Published by the sampler and copied to mShownSprites together with
    // mShownText, because a line index only means something for the text it came from.
    struct PanelSprites {
        int   healthLine = -1;   // line the hearts go on, -1 = none
        int   armorLine  = -1;   // line the armour sprites go on, -1 = none
        float health     = 0.0f; // hit points to fill the hearts with
        float healthMax  = 0.0f; // hit points at full health = number of hearts
        float armor      = 0.0f; // armour points to fill the armour icons with
        int   perRow     = 10;   // sprites per row (EntityOptions::heartsPerRow)
        // "label number" of each row, written behind the sprites in the "+number" styles.
        // Deliberately *not* the line's own text, which may be a bar of block characters.
        std::string healthText;
        std::string armorText;

        // A container's contents, drawn as the game's own item icons on the line the
        // container adapter wrote (BlockExtrasConfig::chestIcons): the items themselves,
        // plus how many fit on a row and how many are drawn at all. Empty for anything
        // that is not a container, and on the server, where the plain line stays.
        int                        itemsLine   = -1;
        std::vector<ContainerItem> items;
        int                        itemsPerRow = 9;
        int                        itemsLimit  = 18;
        // A line the client drops entirely: an empty container has nothing to show, so its
        // "Items 0/27" line is left out rather than kept as the fallback. A server, whose
        // channel carries text and nothing else, keeps the line.
        int hiddenLine = -1;
    };
    PanelSprites mSprites;       // the current subject's sprite rows
    PanelSprites mShownSprites;  // the ones the drawn text belongs to
    // The level the caches belong to. Leaving a world and loading another one reuses block
    // positions and level addresses, so every cache keyed by those is dropped in between.
    void const* mLastLevel = nullptr;

    /// Turns the container the extras builder reported (mContainer) into the panel's own
    /// item icon row: the line of the drawn text it replaces, the items themselves and how
    /// they are laid out. Does nothing when the option keeps the plain "Items n/m" line,
    /// when the subject carries no container, or when that container is empty.
    void setContainerIcons(PanelSprites& sprites, int extrasLine, Config const& cfg);
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
