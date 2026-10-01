#include "ClientLogic.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <string>
#include <variant>
#include <vector>

#include "mc/client/game/ClientInstance.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/gui/CaretMeasureData.h"
#include "mc/client/gui/Font.h"
#include "mc/client/gui/FontHandle.h"
#include "mc/client/gui/TextAlignment.h"
#include "mc/client/gui/TextMeasureData.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "mc/client/gui/FontRepository.h"
#include "mc/client/renderer/BaseActorRenderContext.h"
// Needed by drawPanelNative: the game object behind getMinecraftGame_DEPRECATED() and the
// result type of the UI's text measurement. Added after the line above rather than before it,
// so that header keeps the include order it needs.
#include "mc/client/game/IMinecraftGame.h"
#include "mc/client/gui/controls/MeasureResult.h"
#include "mc/client/renderer/actor/ItemRenderer.h"
#include "mc/deps/core/math/Color.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/deps/input/RectangleArea.h"
#include "mc/deps/minecraft_renderer/resources/UIStructureVolumeOffscreenCaptureDescription.h"
#include "mc/deps/minecraft_renderer/resources/UIThumbnailMeshOffscreenCaptureDescription.h"
#include "mc/locale/I18n.h"
#include "mc/locale/Localization.h"
#include "mc/math/vector/Vecs.h"
#include "mc/network/GameConnectionInfo.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/containers/ContainerEnumName.h"
#include "mc/world/containers/managers/models/ContainerManagerModel.h"
#include "mc/world/containers/models/ContainerModel.h"
#include "mc/world/inventory/network/ContainerScreenContext.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/actor/BlockActor.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/item/Item.h"

#include "Config.h"
#include "ConfigUi.h"
#include "EntityTarget.h"
#include "Extras.h"
#include "Format.h"
#include "I18n.h"
#include "Insight.h"
#include "InsightCommand.h"
#include "Raycast.h"
#include "Util.h"

#include "ll/api/event/command/ClientCommandRegisterEvent.h"
#include "ll/api/event/input/KeyInputEvent.h"
#include "ll/api/event/input/MouseInputEvent.h"
#include "ll/api/input/KeyRegistry.h"

namespace insight {

namespace {

// parse "rrggbb" hex into rgb components 0..1
bool parseHexColor(std::string const& hex, float& r, float& g, float& b) {
    auto hx = [](char c) -> int {
        if (c >= '0' && c <= '9') {
            return c - '0';
        }
        if (c >= 'a' && c <= 'f') {
            return c - 'a' + 10;
        }
        if (c >= 'A' && c <= 'F') {
            return c - 'A' + 10;
        }
        return -1;
    };
    if (hex.size() != 6) {
        return false;
    }
    int rv = (hx(hex[0]) << 4) | hx(hex[1]);
    int gv = (hx(hex[2]) << 4) | hx(hex[3]);
    int bv = (hx(hex[4]) << 4) | hx(hex[5]);
    if (rv < 0 || gv < 0 || bv < 0) {
        return false;
    }
    r = rv / 255.0f;
    g = gv / 255.0f;
    b = bv / 255.0f;
    return true;
}

// One colorized run of text (a § color code was resolved already).
struct ColorRun {
    std::string text;
    float       r = 1.0f;
    float       g = 1.0f;
    float       b = 1.0f;
};

// The vanilla 16 legacy color codes.
[[nodiscard]] char const* legacyPalette(int code) {
    static constexpr std::array<const char*, 16> palette = {
        "000000", // 0 black
        "0000AA", // 1 dark_blue
        "00AA00", // 2 dark_green
        "00AAAA", // 3 dark_aqua
        "AA0000", // 4 dark_red
        "AA00AA", // 5 dark_purple
        "FFAA00", // 6 gold
        "AAAAAA", // 7 gray
        "555555", // 8 dark_gray
        "5555FF", // 9 blue
        "55FF55", // a green
        "55FFFF", // b aqua
        "FF5555", // c red
        "FF55FF", // d light_purple
        "FFFF55", // e yellow
        "FFFFFF", // f white
    };
    if (code < 0 || code > 15) {
        return palette[15];
    }
    return palette[code];
}

// Splits a §-color-coded string into plain-text runs, each with the active
// color. Formatting codes we cannot render in the UI font (k/l/m/n/o) are
// stripped; "§r" resets to `defaultR/G/B`. Returns the total plain text
// length (excluding codes) in `outPlainLength`.
[[nodiscard]] std::vector<ColorRun>
splitColorRuns(std::string const& s, float defaultR, float defaultG, float defaultB, size_t& outPlainLength) {
    std::vector<ColorRun> runs;
    ColorRun              cur;
    cur.r          = defaultR;
    cur.g          = defaultG;
    cur.b          = defaultB;
    outPlainLength = 0;

    auto flush = [&] {
        if (!cur.text.empty()) {
            runs.push_back(std::move(cur));
            cur   = ColorRun{};
            cur.r = defaultR;
            cur.g = defaultG;
            cur.b = defaultB;
        }
    };

    // The text mixes real UTF-8 (block names, CJK) with legacy single-byte
    // section signs (0xA7) from `§` colour codes. Matching the raw
    // byte 0xA7 is wrong: it also occurs *inside* valid UTF-8 characters
    // (性 = E6 80 A7, 槽 = E6 A7 BD), and treating those as color codes ate the
    // rest of the name (黏性活塞 -> 黏塞, 槽位 -> ?位). So decode UTF-8 properly
    // and only accept a section sign when the decoded character really is
    // U+00A7 (UTF-8 "§" = C2 A7) or a lone 0xA7 that cannot start a character.
    auto applyCode = [&](char code) {
        char c = static_cast<char>(std::tolower(static_cast<unsigned char>(code)));
        flush();
        if (c == 'r') {
            cur.r = defaultR;
            cur.g = defaultG;
            cur.b = defaultB;
            return;
        }
        if (c >= '0' && c <= '9') {
            float cr, cg, cb;
            parseHexColor(legacyPalette(c - '0'), cr, cg, cb);
            cur.r = cr;
            cur.g = cg;
            cur.b = cb;
            return;
        }
        if (c >= 'a' && c <= 'f') {
            float cr, cg, cb;
            parseHexColor(legacyPalette(c - 'a' + 10), cr, cg, cb);
            cur.r = cr;
            cur.g = cg;
            cur.b = cb;
            return;
        }
        // formatting (k/l/m/n/o): not renderable in this font, ignored
    };

    size_t i = 0;
    while (i < s.size()) {
        auto b = static_cast<unsigned char>(s[i]);

        // Decode one whole UTF-8 character at i (with continuation checks).
        size_t   len   = 0;
        uint32_t cp    = b;
        bool     valid = true;
        if (b < 0x80) {
            len = 1;
        } else if ((b & 0xE0) == 0xC0) {
            len = 2;
            cp  = b & 0x1Fu;
        } else if ((b & 0xF0) == 0xE0) {
            len = 3;
            cp  = b & 0x0Fu;
        } else if ((b & 0xF8) == 0xF0) {
            len = 4;
            cp  = b & 0x07u;
        } else {
            valid = false;
        }
        if (valid && len > 1) {
            if (i + len > s.size()) {
                valid = false;
            } else {
                for (size_t k = 1; k < len; ++k) {
                    auto c = static_cast<unsigned char>(s[i + k]);
                    if ((c & 0xC0) != 0x80) {
                        valid = false;
                        break;
                    }
                    cp = (cp << 6) | (c & 0x3Fu);
                }
            }
        }

        if (valid && cp == 0x00A7u) { // UTF-8 "§": the next byte is the code
            if (i + len >= s.size()) {
                break; // dangling section sign: drop it
            }
            applyCode(s[i + len]);
            i += len + 1;
            continue;
        }
        if (!valid && b == 0xA7) { // legacy single-byte section sign
            if (i + 1 >= s.size()) {
                break;
            }
            applyCode(s[i + 1]);
            i += 2;
            continue;
        }
        if (!valid) { // malformed byte: keep it verbatim
            cur.text.push_back(s[i]);
            ++outPlainLength;
            ++i;
            continue;
        }
        for (size_t k = 0; k < len; ++k) { // ordinary character: copy its bytes
            cur.text.push_back(s[i + k]);
            ++outPlainLength;
        }
        i += len;
    }
    flush();
    return runs;
}

// The dimension carries its own name ("overworld" / "nether" / "the_end"),
// so no id -> name table is needed.
std::string clientDimName(BlockSource const& region) {
    try {
        return region.getDimension().mName;
    } catch (...) {
        return std::to_string(static_cast<int>(region.getDimensionId()));
    }
}

// While a container screen (chest/barrel/shulker/...) is open, the engine keeps
// live container models for the UI, whereas the world copy we normally read
// keeps its old contents until the chunk is reloaded. This inspects every model
// the open container manager holds, logs what it sees (debug) and snapshots the
// block-container one so the panel shows the player's edits after closing.
void captureOpenContainer(IClientInstance& client, std::string const& screenName) {
    auto& logger = Insight::getInstance().getSelf().getLogger();

    // throttle the dump to once per second
    static std::chrono::steady_clock::time_point lastDump{};
    auto                                         now  = std::chrono::steady_clock::now();
    bool                                         dump = now - lastDump > std::chrono::seconds(1);
    if (dump) {
        lastDump = now;
        logger.debug("[ctr] screen={}", screenName);
    }

    try {
        auto* player = client.getLocalPlayer();
        if (!player) {
            if (dump) {
                logger.debug("[ctr] no local player");
            }
            return;
        }
        auto manager = player->mContainerManager;
        if (!manager) {
            if (dump) {
                logger.debug("[ctr] no container manager");
            }
            return;
        }

        auto const& screenContext = *manager->mScreenContext;
        auto const& owner         = *screenContext.mOwner;
        auto const* pos           = std::get_if<BlockPos>(&owner);
        auto const* ownerId       = std::get_if<ActorUniqueID>(&owner);
        if (dump) {
            logger.debug(
                "[ctr] blockPos={}",
                pos ? (std::to_string(pos->x) + "," + std::to_string(pos->y) + "," + std::to_string(pos->z))
                    : std::string("<none>")
            );
        }

        // Candidates are the engine's live per-screen container models. The
        // block's own container is the "container_items" model (enum 7,
        // LevelEntityContainer); barrel/shulker/crafter screens use their own
        // enum but the same key. The player's inventory models share this map,
        // so they are filtered out.
        // Which model is the block's own container? Matching the key only worked
        // for chests: a brewing stand screen keys its model differently, so its
        // panel kept showing the stale world copy (the "brewing stand never
        // updates" bug). The block's container tells us how many slots the model
        // must have, which identifies it whatever it is called.
        int blockSlots = -1;
        if (pos) {
            if (auto const* blockActor = screenContext.tryGetBlockActor()) {
                if (auto const* container = blockContainerOf(blockActor)) {
                    blockSlots = container->getContainerSize();
                }
            }
            if (blockSlots < 0) {
                // some screens (brewing stands) do not hand out their block actor;
                // the block's own container still says how many slots the model
                // must have, which is all that is needed to recognise it
                if (auto* region = client.getRegion()) {
                    if (auto const* container = blockContainerOf(region->getBlockEntity(*pos))) {
                        blockSlots = container->getContainerSize();
                    }
                }
            }
        }
        // Some screens spread the block's slots over several models: a brewing
        // stand has one for the fuel, one for the input and one for the results
        // (brewing_fuel_item / brewing_input_item / brewing_result_items). Their
        // keys start with the screen's own name ("brewing_stand_screen" ->
        // "brewing"), which is engine naming, and their sizes add up to the
        // block's slot count - so they are summed and matched as one container.
        std::string screenPrefix = screenName;
        if (auto cut = screenPrefix.find("_screen"); cut != std::string::npos) {
            screenPrefix.resize(cut);
        }
        if (auto cut = screenPrefix.find('_'); cut != std::string::npos) {
            screenPrefix.resize(cut);
        }
        int splitFilled = 0;
        int splitSize   = 0;
        int splitTotal  = 0;

        int bestFilled = -1;
        int bestSize   = 0;
        int bestTotal  = 0;
        for (auto const& [key, model] : *manager->mContainers) {
            if (!model) {
                continue;
            }
            // ContainerModel::getItemStack()/getItems() are the live per-slot
            // stacks the UI is using; model->_getContainer() returns the stale
            // world copy, which is why an edit inside the UI was invisible.
            int size   = model->getContainerSize();
            int filled = 0;
            int total  = 0;
            if (size > 0) {
                for (int i = 0; i < size; ++i) {
                    auto const& stack = model->getItemStack(i);
                    if (!stack.isNull()) {
                        ++filled;
                        total += stack.mCount;
                    }
                }
            }
            if (!screenPrefix.empty() && key.rfind(screenPrefix, 0) == 0) {
                splitSize   += size;
                splitFilled += filled;
                splitTotal  += total;
            }
            bool preferred = key == "container_items" || (blockSlots > 0 && size == blockSlots);
            if (dump) {
                logger.debug(
                    "[ctr]   model '{}' size={} filled={}{}",
                    key,
                    size,
                    filled,
                    preferred ? "  <-- live block container" : ""
                );
            }
            if (preferred && size > 0 && (bestFilled < 0 || size > bestSize)) {
                bestSize   = size;
                bestFilled = filled;
                bestTotal  = total;
            }
        }
        // a container the screen split into pieces wins over the single-model
        // guess when its total is exactly the block's slot count (or when nothing
        // else was recognised at all)
        if (splitSize > 0 && ((blockSlots > 0 && splitSize == blockSlots) || bestFilled < 0)) {
            bestSize   = splitSize;
            bestFilled = splitFilled;
            bestTotal  = splitTotal;
        }
        if (!pos && ownerId && bestFilled >= 0) {
            // Container entities (chest/hopper minecart, boat with chest) are
            // keyed by their unique id instead of a block position.
            setLiveEntityContainerSnapshot(static_cast<int64_t>(ownerId->rawID), bestFilled, bestSize, bestTotal);
            logger.debug(
                "[ctr] entity snapshot -> {}/{} total {} (actor {})",
                bestFilled,
                bestSize,
                bestTotal,
                static_cast<int64_t>(ownerId->rawID)
            );
        }
        if (pos && bestFilled >= 0) {
            setLiveContainerSnapshot(*pos, bestFilled, bestSize, bestTotal);
            // Log every change immediately (not throttled) so a single edit
            // inside the container UI is always visible in the log.
            static int lastFilled = -1;
            static int lastSize   = -1;
            static int lastTotal  = -1;
            if (bestFilled != lastFilled || bestSize != lastSize || bestTotal != lastTotal) {
                logger.debug(
                    "[ctr] live container changed: {}/{} total {} (was {}/{}, total {})",
                    bestFilled,
                    bestSize,
                    bestTotal,
                    lastFilled,
                    lastSize,
                    lastTotal
                );
                lastFilled = bestFilled;
                lastSize   = bestSize;
                lastTotal  = bestTotal;
            }
        }
    } catch (...) {
        if (dump) {
            logger.debug("[ctr] exception while reading container");
        }
    }
}

} // namespace

// Destructor intentionally empty: teardown happens in disable(), which the
// mod lifecycle calls before the loader releases the module. During DLL
// detach / process exit nothing here must touch the loader again.
ClientLogic::~ClientLogic() = default;

bool ClientLogic::enable() {
    auto& logger = Insight::getInstance().getSelf().getLogger();

    auto& bus = ll::event::EventBus::getInstance();
    mListeners.emplace_back(bus.emplaceListener<ll::event::render::BeforeUIRenderEvent>(
        [this](ll::event::render::BeforeUIRenderEvent& event) { onRender(event); }
    ));
    // /cliinsight on the client: status / toggle / on / off / reload / set / gui.
    // on/off/toggle flip `showOverlay`, the same value the hotkey writes, so the
    // command and the key stay in sync and both persist the change.
    auto const toggle = [](Player&) -> bool {
        auto const& cfg = Insight::cfg();
        (void)Insight::applyConfigEdit("showOverlay", cfg.client.showOverlay ? "false" : "true", {});
        // report what the config actually says, not what we asked for
        return Insight::cfg().client.showOverlay;
    };
    // The subject's icon is drawn by the game's own item renderer, after the screen's UI so
    // it lands on top of it (see onAfterRender).
    mListeners.emplace_back(bus.emplaceListener<ll::event::render::AfterUIRenderEvent>(
        [this](ll::event::render::AfterUIRenderEvent& event) { onAfterRender(event); }
    ));
    mListeners.emplace_back(bus.emplaceListener<ll::event::command::ClientCommandRegisterEvent>(
        [toggle](auto&) {
            registerInsightCommand(true, toggle, [] { ConfigUi::instance().setVisible(true); });
        }
    ));

    // configuration screen: the overlay draws it inside its ImGui frame
    mOverlay.setWindowDrawer([] { ConfigUi::instance().draw(); });

    // keyboard/mouse for that screen. While it is open the events are cancelled,
    // so the game neither moves the player nor looks around (the screen is
    // modal). Events arrive on the game thread and are replayed into ImGui on
    // the render thread by ConfigUi.
    //
    // The hotkeys are handled here as well: the KeyRegistry handlers alone did
    // not fire reliably, while these input events always arrive, and the handles
    // are still consulted for their *current* key codes so remapping them in the
    // game's key settings keeps working.
    auto& keys    = ll::input::KeyRegistry::getInstance();
    auto& openKey = keys.getOrCreateKey("Insight.openConfig", {Insight::cfg().client.keyOpenConfig}, true);
    auto& showKey = keys.getOrCreateKey("Insight.toggleShow", {Insight::cfg().client.keyToggleShow}, true);

    mListeners.emplace_back(bus.emplaceListener<ll::event::input::KeyInputEvent>(
        [&openKey, &showKey](ll::event::input::KeyInputEvent& event) {
            auto& ui = ConfigUi::instance();
            if (event.isDown()) {
                int const code = event.keyCode();
                for (int bound : openKey.getKeyCodes()) {
                    if (bound != 0 && bound == code) {
                        ui.toggle();
                        event.cancel();
                        return;
                    }
                }
                for (int bound : showKey.getKeyCodes()) {
                    if (bound != 0 && bound == code) {
                        auto const& cfg = Insight::cfg();
                        (void)Insight::applyConfigEdit("showOverlay", cfg.client.showOverlay ? "false" : "true", {});
                        event.cancel();
                        return;
                    }
                }
            }
            if (!ui.visible()) {
                return;
            }
            ui.onKey(event.keyCode(), event.isDown());
            event.cancel();
        }
    ));
    mListeners.emplace_back(
        bus.emplaceListener<ll::event::input::MouseInputEvent>([](ll::event::input::MouseInputEvent& event) {
            // The configuration screen reads the mouse through ImGui's Win32
            // backend, so while it is open the game must not see the mouse at all.
            if (!ConfigUi::instance().visible()) {
                return;
            }
            event.cancel();
        })
    );

    // The info panel is drawn by the ImGui overlay (vanilla UI text cannot
    // render CJK names on this GDK build). Hooks are installed once here and
    // the backend is bootstrapped lazily on the first game Present.
    if (!mOverlay.install()) {
        logger.warn("ImGui overlay hooks not installed yet; retrying on next reload");
    }

    logger.info("Client mode enabled (GDK). overlay={}", Insight::cfg().client.showOverlay);
    return true;
}

void ClientLogic::disable() {
    auto& bus = ll::event::EventBus::getInstance();
    for (auto& l : mListeners) {
        bus.removeListener(l);
    }
    mListeners.clear();
    mVisible = false;
    mText.clear();
    mOverlay.shutdown();
}

void ClientLogic::pushOverlay() {
    auto const& cfg         = Insight::cfg();
    bool        wantVisible = mVisible && !mText.empty();

    ImGuiOverlay::Content content;
    // The HUD is drawn natively (ClientLogic::drawPanelNative); the ImGui overlay is kept for
    // the configuration screen only, so its HUD content is never shown.
    content.visible         = false;
    content.anchor          = cfg.client.anchor;
    content.offsetX         = cfg.client.offsetX;
    content.offsetY         = cfg.client.offsetY;
    content.fontSize        = std::max(0.25f, cfg.client.fontSize);
    content.background      = cfg.client.background;
    content.backgroundAlpha = cfg.client.backgroundAlpha;
    content.shadow          = cfg.client.shadow;
    content.maxWidth        = cfg.client.maxWidth;
    content.transitionTime  = cfg.client.transitionTime;
    content.targetKey       = mTargetKey;
    // An icon slot only when there is something the item renderer can draw: an empty or
    // entity subject leaves the panel as it was.
    content.icon            = !mIconStack.isNull();

    if (wantVisible) {
        float textR = 1.0f, textG = 1.0f, textB = 1.0f;
        parseHexColor(cfg.client.textColor, textR, textG, textB);
        auto rawLines = util::splitLines(mText);
        content.lines.reserve(rawLines.size());
        for (auto const& line : rawLines) {
            size_t                         plainLen = 0;
            auto                           runs     = splitColorRuns(line, textR, textG, textB, plainLen);
            std::vector<ImGuiOverlay::Run> overlayRuns;
            overlayRuns.reserve(runs.size());
            for (auto const& run : runs) {
                overlayRuns.push_back(ImGuiOverlay::Run{run.text, run.r, run.g, run.b});
            }
            content.lines.emplace_back(std::move(overlayRuns));
        }
    }
    mOverlay.setContent(std::move(content));
}

void ClientLogic::onRender(ll::event::render::BeforeUIRenderEvent& event) {
    auto const& cfg = Insight::cfg();

    // The overlay hooks can only be installed once the game window exists.
    // enable() may run before that, so retry here (throttled) instead of
    // leaving the overlay dead until the next reload.
    if (!mOverlay.installed()) {
        auto now = std::chrono::steady_clock::now();
        if (now - mLastInstallAttempt > std::chrono::seconds(2)) {
            mLastInstallAttempt = now;
            mOverlay.install();
        }
    }
    // Configuration screen: it renders on the overlay's thread, so it only
    // queues edits - they are applied here, on the game thread, together with
    // the config file write. It is handled *before* the "mod or panel switched
    // off" shortcut below, because those are options the screen itself edits.
    {
        auto&       ui   = ConfigUi::instance();
        std::string lang = Insight::cfg().client.language;
        lang             = insight::resolveLanguageCode(lang, [] {
            try {
                return getI18n().getCurrentLanguage()->getFullLanguageCode();
            } catch (...) {
                return std::string{};
            }
        }());
        std::string option;
        std::string value;
        while (ui.takeEdit(option, value)) {
            if (option == "reload") {
                Insight::reloadConfigFromDisk();
                ui.setStatus(true, tr(lang, "Insight configuration reloaded."));
            } else {
                auto const result = Insight::applyConfigEdit(option, value, lang);
                ui.setStatus(result.ok, result.message);
            }
        }
        ui.setLocale(lang);
        ui.setPreviewText(mText);
    }
    if (!cfg.enabled || !cfg.client.showOverlay) {
        mVisible = false;
        pushOverlay();
        return;
    }

    auto& ctx         = event.uiRenderContext();
    auto& client      = ctx.mClient; // IClientInstance&
    auto* localPlayer = client.getLocalPlayer();


    if (!localPlayer || !client.getLevel()) {
        mVisible          = false; // in a menu / loading / outside any level
        mRefreshRequested = true;
        pushOverlay();
        return;
    }

    // ------------------------------------------------------------------
    // Screen-layer handling. The client fires a UI render event once per
    // frame for EVERY screen in the stack (observed in-game: hud_screen,
    // toast_screen, debug_screen, pause_screen, ...), not only for the HUD.
    // Only the hud_screen pass may drive sampling / visibility: the toast
    // and debug layers render on top of the HUD *every frame* during normal
    // gameplay, and letting them clear the visibility state here made the
    // overlay vanish right after each sample (it only came back on the next
    // sampling frame -> one flash per refresh).
    std::string screen;
    try {
        screen = event.screenView().getScreenName();
    } catch (...) {
        screen.clear();
    }
    bool hudLike = screen.empty() || screen == "hud_screen" || screen.find("hud") != std::string::npos;

    // Transparent HUD overlays that are always present during gameplay:
    // ignore them completely (never touch visibility state).
    if (!hudLike && (screen == "toast_screen" || screen == "debug_screen")) {
        return;
    }
    if (!hudLike) {
        // A real (menu) screen is part of the stack. Remember it with a
        // debounce instead of clearing mVisible right away, so a menu that
        // only shows up briefly cannot tear the overlay down. While a
        // container screen is open we also snapshot its live contents (the
        // world copy of a container stays stale until the chunk reloads).
        mHudStreak = 0;
        captureOpenContainer(client, screen);
        return;
    }

    // ---- hud_screen pass from here on (runs once per frame) ------------
    constexpr int kGuiDebounceHudFrames = 3; // pure hud passes after the last menu event
    if (mHudStreak < kGuiDebounceHudFrames) {
        ++mHudStreak;
    }

    // Multiplayer handling: "off" = rely on the server channel while online
    // (the client cannot read server-authoritative data anyway).
    if (cfg.client.overlayOnRemote == "off") {
        bool remote = false;
        try {
            remote = client.getGameConnectionInfo().has_value();
        } catch (...) {
            remote = false;
        }
        if (remote) {
            mVisible          = false;
            mRefreshRequested = true;
            pushOverlay();
            return;
        }
    }

    // Like Jade: keep the overlay hidden while a game UI screen is open.
    // Menu events above reset the streak; we need a few consecutive pure
    // hud passes before drawing resumes (single-frame noise is debounced).
    if (cfg.client.hideOverlayInGui && mHudStreak < kGuiDebounceHudFrames) {
        mVisible          = false;
        mRefreshRequested = true; // re-sample as soon as the GUI closes
        pushOverlay();
        return;
    }

    // Sampling. The look ray is cast every frame so we can (a) refresh
    // immediately when the crosshair moves to another block and (b) refresh
    // right after a GUI (container etc.) was closed - container contents and
    // block states otherwise keep showing the pre-GUI values until the next
    // cadence tick, and for containers the client data is only re-read then.
    auto const samplePeriod = std::chrono::milliseconds(std::max(1, cfg.intervalTicks) * 50);
    auto       now          = std::chrono::steady_clock::now();

    // "[look]" diagnostics: remember what was logged last, so the debug log gets
    // one line per change instead of one line per sampling round
    static std::string lastBlockLog;
    static std::string lastEntityLog;

    Vec3  origin = localPlayer->getEyePos();
    Vec3  dir    = localPlayer->getViewVector(0.0f);
    auto& region = localPlayer->getDimensionBlockSource();
    auto  hit    = raycastToBlock(region, origin, dir, static_cast<double>(cfg.maxDistance), cfg.passThroughLiquids);

    bool targetChanged = hit ? !(mHadHit && mLastHitPos == hit->pos && mLastHitType == hit->typeName) : mHadHit;
    bool forceRefresh  = mRefreshRequested;
    mRefreshRequested  = false;

    std::string text;
    if (forceRefresh || targetChanged || now - mLastSample >= samplePeriod) {
        mLastSample = now;
        mHadHit     = hit.has_value();
        if (hit) {
            mLastHitPos  = hit->pos;
            mLastHitType = hit->typeName;
        }

        // UI language, used for the built-in extra labels
        std::string lang = cfg.client.language;
        lang             = insight::resolveLanguageCode(lang, [] {
            try {
                return getI18n().getCurrentLanguage()->getFullLanguageCode();
            } catch (...) {
                return std::string{};
            }
        }());

        // nearest entity on the same look ray; wins when it is closer than a
        // hit block (client-side world data, best effort on remote servers)
        Actor* entity     = nullptr;
        double entityDist = 0;
        if (cfg.entityEnabled) {
            auto entHit = findLookEntity(region, &*localPlayer, origin, dir, cfg.maxDistance);
            entity      = entHit.actor;
            entityDist  = entHit.distance;
            // A painting or an item frame hangs on the block face, so its hit
            // distance equals the block's: it has to win that tie or its extras
            // are never reached. Anything hidden behind a block is at least a
            // block away, so a small tolerance cannot pick up the wrong entity.
            if (entity && entityDist > (hit ? hit->distance + 0.1 : 1e30)) {
                entity = nullptr; // the block in front wins
            }
        }

        if (entity) {
            std::string eType = entity->getTypeName();
            bool        hasHp = entityHasHealth(*entity);
            auto        info  = makeEntityLookInfo(
                entityDisplayName(entity, lang),
                eType,
                hasHp ? entity->getHealth() : 0,
                hasHp ? entity->getMaxHealth() : 0
            );
            info.distance = entityDist;
            info.dimName  = clientDimName(region);
            info.extras   = buildEntityExtras(region, *entity, lang, cfg.extras);
            // The entity panel has its own layout (renderEntityText): the entity's own
            // yaw as the facing, its position, and hit points - but no light level and
            // no emission, which describe a world position, not the entity.
            info.entityKey = entityDisplayKey(entity);
            info.direction = describeEntityFacing(*entity, lang);
            {
                auto const& epos = entity->getPosition();
                info.x           = static_cast<int>(std::floor(epos.x));
                info.y           = static_cast<int>(std::floor(epos.y));
                info.z           = static_cast<int>(std::floor(epos.z));
            }
            text       = renderEntityText(cfg, info, lang);
            mTargetKey = "e:" + eType + "#" + std::to_string(entity->getOrCreateUniqueID().rawID);
            mIconStack = ItemStack{}; // entities have no block icon
            std::string const entityState = info.entityType + "|" + info.entityName + "|" + std::to_string(info.health);
            if (entityState != lastEntityLog) {
                lastEntityLog = entityState;
                Insight::getInstance().getSelf().getLogger().debug(
                    "[look] entity={} name={} hp={}/{}",
                    info.entityType,
                    info.entityName,
                    info.health,
                    info.maxHealth
                );
            }
        } else if (hit) {
            auto info      = makeBlockLookInfo(*hit, lang);
            info.dimName   = clientDimName(region);
            info.extras    = buildBlockExtras(region, hit->pos, hit->typeName, lang, cfg.extras);
            info.direction = describeBlockFacing(region, hit->pos, lang);
            info.light     = describeBlockLight(region, hit->pos);
            info.emission  = describeBlockEmission(region, hit->pos);
            text       = renderBlockText(cfg, info, lang);
            mTargetKey = "b:" + hit->typeName + "@" + std::to_string(hit->pos.x) + "," + std::to_string(hit->pos.y)
                       + "," + std::to_string(hit->pos.z);
            // The same subject as an item stack, which is what the item renderer draws from.
            // The subject in its *item* form, which is what the game's item renderer draws.
            // Lamium resolves a block target the same way (Block::asItemInstance -> full name
            // + aux -> ItemStack::reinit) and keeps that stack for as long as the target lives.
            // Rebuilding it every sample would restart the model's pickup animation, which is
            // what made 3D block icons pop while 2D item sprites looked fine. Blocks without an
            // item (portal, fire, ...) simply get no icon.
            if (mIconKey != mTargetKey) {
                mIconKey   = mTargetKey;
                mIconStack = ItemStack{};
                try {
                    auto item = region.getBlock(hit->pos).asItemInstance(region, hit->pos, true);
                    if (!item.isNull() && item.mItem) {
                        mIconStack.reinit(item.mItem->mFullName->getString(), 1, item.getAuxValue());
                        // A freshly reinited stack counts as "just picked up", so the item
                        // renderer replays its pickup squash on it - one visible pop on every
                        // target switch, on 3D block models only (2D sprites are pose
                        // independent). Lamium clears the same two flags (InfoHud.cpp:137-138).
                        mIconStack.mShowPickUp  = false;
                        mIconStack.mWasPickedUp = false;
                    }
                } catch (...) {
                    mIconStack = ItemStack{};
                }
            }
            std::string neighborInfo;
            if (hit->typeName.find("piston") != std::string::npos) {
                // log the six neighbours so the piston-arm block id is visible
                static constexpr int offsets[6][3] = {
                    {0,  -1, 0 },
                    {0,  1,  0 },
                    {0,  0,  -1},
                    {0,  0,  1 },
                    {-1, 0,  0 },
                    {1,  0,  0 }
                };
                for (auto const& o : offsets) {
                    BlockPos nb(hit->pos.x + o[0], hit->pos.y + o[1], hit->pos.z + o[2]);
                    neighborInfo += " " + region.getBlock(nb).getTypeName();
                }
            }
            // diagnostics: one line per *change* of the sampled target
            std::string const blockState = info.blockType + "|" + info.blockName + "|" + info.extras + "|"
                                         + info.direction + "|" + info.light + "|" + info.emission + "|"
                                         + std::to_string(hit->pos.x) + "," + std::to_string(hit->pos.y) + ","
                                         + std::to_string(hit->pos.z) + neighborInfo;
            if (blockState != lastBlockLog) {
                lastBlockLog = blockState;
                Insight::getInstance().getSelf().getLogger().debug(
                    "[look] type={} key={} name={} extras={} | states={} | dir={} light={} emit={}{}",
                    info.blockType,
                    info.blockKey,
                    info.blockName,
                    info.extras,
                    describeBlockStateNames(region, hit->pos),
                    info.direction,
                    info.light,
                    info.emission,
                    neighborInfo.empty() ? std::string() : " | neighbors:" + neighborInfo
                );
            }
        } else if (cfg.showEmpty) {
            text       = renderEmptyText(cfg);
            mTargetKey = "empty";
        }
        mText    = text;
        mVisible = !text.empty();
    }

    pushOverlay();
}

void ClientLogic::onAfterRender(ll::event::render::AfterUIRenderEvent& event) {
    auto const& cfg = Insight::cfg();
    // Whatever happens below, the panel must not keep a stale hole from the previous frame.
    mOverlay.setIconHole({});
    if (!cfg.enabled || !cfg.client.showOverlay || !mVisible) {
        return;
    }
    // Only the hud_screen pass owns the panel, like in onRender.
    std::string screen;
    try {
        screen = event.screenView().getScreenName();
    } catch (...) {
        return;
    }
    bool const hudLike = screen.empty() || screen == "hud_screen" || screen.find("hud") != std::string::npos;
    if (!hudLike) {
        return;
    }
    // Draw on the HUD pass only. This event fires once per screen in the stack, so a looser
    // filter paints the panel (and the icon) several times a frame; the 3D block model takes
    // its pose from the context state, so repeated draws in one frame make it appear to jump,
    // while 2D item sprites - which are pose independent - look fine.
    if (screen == "hud_screen") {
        drawPanelNative(event);
    }
}

void ClientLogic::drawPanelNative(ll::event::render::AfterUIRenderEvent& event) {
    auto const& cfg     = Insight::cfg();
    auto&       context = event.uiRenderContext();
    auto&       client  = context.mClient;

    // Fade with the display, advanced by elapsed time: this event fires once per screen in the
    // stack, so a fixed per-call step would fade several times as fast.
    auto const  now    = std::chrono::steady_clock::now();
    float const elapsed =
        mLastPanelTick.time_since_epoch().count() == 0
            ? 0.0f
            : std::chrono::duration<float>(now - mLastPanelTick).count();
    mLastPanelTick     = now;
    float const wanted = mVisible && !mText.empty() ? 1.0f : 0.0f;
    float const speed  = cfg.client.transitionTime > 0.001f ? 1.0f / cfg.client.transitionTime : 1000.0f;
    float const delta  = std::clamp(elapsed * speed, 0.0f, 1.0f);
    mNativeFade = wanted > mNativeFade ? std::min(wanted, mNativeFade + delta) : std::max(wanted, mNativeFade - delta);
    // Draw the newest content, but keep the previous one while the panel fades out: the sampler
    // empties mText as soon as the subject is lost, so returning on that dropped the last lines
    // and the box in a single frame - a blink where the fade should be.
    if (!mText.empty()) {
        mShownText = mText;
    }
    if (mNativeFade <= 0.001f) {
        mShownText.clear();
        return;
    }
    if (mShownText.empty()) {
        return;
    }

    // The same "default" font a vanilla label resolves to - CJK included, which is what the
    // earlier attempt got wrong by feeding the UI context text it never measured.
    auto const& fontHandle = client.getMinecraftGame_DEPRECATED().getFontRepository()->getFontFromFontType("default");
    Font&       font       = fontHandle.getFont();
    Bedrock::NotNullNonOwnerPtr<FontHandle const> fontRef{
        Bedrock::NonOwnerPointer<FontHandle const>{fontHandle}
    };
    auto& measure = context.getMeasureStrategy();

    float const            fontScale = std::max(0.25f, cfg.client.fontSize);
    TextMeasureData const  textData{fontScale, 0.0f, cfg.client.shadow, false, false, ::ui::TextAlignment::Left};
    CaretMeasureData const caretData{-1, false};

    float textR = 1.0f;
    float textG = 1.0f;
    float textB = 1.0f;
    parseHexColor(cfg.client.textColor, textR, textG, textB);
    mce::Color const textColor{textR, textG, textB, 1.0f};

    // Laying the lines out here, with the engine's own measuring strategy, is what makes the
    // icon cell and the text agree: both are placed in UI units in this one function.
    std::vector<std::string> lines;
    std::vector<float>       widths;
    float                    widest   = 0.0f;
    float                    lineStep = 0.0f;
    // The lines are handed to the engine's text renderer as they are, section-sign colour codes
    // included: feeding them through unmodified is how we find out whether the engine resolves
    // them itself. The old path stripped them here because it drew them as glyphs.
    for (auto& raw : util::splitLines(mShownText)) {
        lines.push_back(raw);
        auto const measured = measure.measureText(fontRef, lines.back(), 4096.0f, 4096.0f, textData, caretData);
        widths.push_back(measured.mSize->x);
        widest   = std::max(widest, measured.mSize->x);
        lineStep = std::max(lineStep, measured.mSize->y * 1.35f);
    }
    if (lineStep <= 0.0f) {
        lineStep = fontScale * 12.0f;
    }

    float const padX = 4.0f;
    float const padY = 3.0f;
    // Two text lines tall, measured from the same line height the text uses, so the icon reads
    // as the subject's headline and still follows client.fontSize.
    float const iconSize = std::round(lineStep * 1.5f);
    bool const  hasIcon  = !mIconStack.isNull() && static_cast<bool>(mIconStack.mItem);
    float const iconCol  = hasIcon ? iconSize + padX : 0.0f;

    glm::vec2 const screen = event.screenView().mSize;
    // Quantised target size, frozen per subject and grow-only: the box - and the icon sitting at
    // its left edge - must not follow every digit a ticking line changes. The subject decides
    // the target once; later updates of the same subject may only make it bigger.
    constexpr float kSizeStep = 8.0f;
    float const     contentW  = widest + 2.0f * padX + iconCol;
    float const     contentH =
        std::max(static_cast<float>(lines.size()) * lineStep, hasIcon ? iconSize : 0.0f) + 2.0f * padY;
    float const wantW = std::ceil(contentW / kSizeStep) * kSizeStep;
    float const wantH = std::ceil(contentH / kSizeStep) * kSizeStep;
    if (mBoxKey != mTargetKey) {
        mBoxKey   = mTargetKey;
        mBoxWantW = wantW;
        mBoxWantH = wantH;
    } else {
        mBoxWantW = std::max(mBoxWantW, wantW);
        mBoxWantH = std::max(mBoxWantH, wantH);
    }
    // The size actually drawn eases towards that target: this is the panel's size-change
    // animation, and it covers a target switch and the same subject growing a line alike. The
    // anchor is applied to the eased size below, so a centred panel also glides while resizing.
    if (mBoxW <= 0.0f || mBoxH <= 0.0f) {
        mBoxW = mBoxWantW; // first appearance: the fade covers it, so no grow-in from nothing
        mBoxH = mBoxWantH;
    } else {
        float const tau = std::max(cfg.client.transitionTime, 0.001f);
        float const k   = 1.0f - std::exp(-elapsed / tau);
        mBoxW += (mBoxWantW - mBoxW) * k;
        mBoxH += (mBoxWantH - mBoxH) * k;
    }
    float const boxW = mBoxW;
    float const boxH = mBoxH;

    // Anchor and offsets, the same semantics the configuration screen offers. offsetY is
    // positive upwards, as in the config.
    std::string const& anchor = cfg.client.anchor;
    float              u      = 0.5f;
    float              v      = 0.5f;
    if (anchor.find("left") != std::string::npos) {
        u = 0.0f;
    }
    if (anchor.find("right") != std::string::npos) {
        u = 1.0f;
    }
    if (anchor.find("top") != std::string::npos) {
        v = 0.0f;
    }
    if (anchor.find("bottom") != std::string::npos) {
        v = 1.0f;
    }
    float const cx = u * screen.x + cfg.client.offsetX * screen.x;
    float const cy = v * screen.y - cfg.client.offsetY * screen.y;

    float left = u <= 0.001f ? cx : (u >= 0.999f ? cx - boxW : cx - boxW / 2.0f);
    float top  = v <= 0.001f ? cy : (v >= 0.999f ? cy - boxH : cy - boxH / 2.0f);
    left       = std::clamp(left, 2.0f, std::max(2.0f, screen.x - boxW - 2.0f));
    top        = std::clamp(top, 2.0f, std::max(2.0f, screen.y - boxH - 2.0f));

    // The rectangle the icon and the text are laid out from. It is derived from the eased size
    // above, so the panel resizes and (with a centred anchor) travels in one motion.
    float const boxL = left;
    float const boxT = top;
    float const boxR = left + boxW;
    float const boxB = top + boxH;

    // Rounded background. The UI context has no rounded primitive (Lamium's ui::card just clips
    // one unit off each corner), so a corner is a staircase of plain fills whose inset follows a
    // quarter circle; they batch with the rest of the UI and are flushed before the icon below.
    auto roundedFill = [&context](
                           float l,
                           float t,
                           float r,
                           float b,
                           mce::Color const& color,
                           float             alpha,
                           float             radius
                       ) {
        if (radius <= 0.5f || r - l <= 2.0f || b - t <= 2.0f) {
            context.fillRectangle(RectangleArea{l, r, t, b}, color, alpha);
            return;
        }
        radius = std::min(radius, std::min((r - l) / 2.0f, (b - t) / 2.0f));
        // Full-height body first, then the top rows and their mirrored bottom rows.
        context.fillRectangle(RectangleArea{l, r, t + radius, b - radius}, color, alpha);
        int const rows = static_cast<int>(std::ceil(radius));
        for (int i = 0; i < rows; ++i) {
            float const y     = static_cast<float>(i);
            float const dy    = radius - (y + 0.5f); // distance from the corner's centre
            float const inset = radius - std::sqrt(std::max(0.0f, radius * radius - dy * dy));
            context.fillRectangle(RectangleArea{l + inset, r - inset, t + y, t + y + 1.0f}, color, alpha);
            context.fillRectangle(RectangleArea{l + inset, r - inset, b - y - 1.0f, b - y}, color, alpha);
        }
    };

    if (cfg.client.background) {
        float const alpha = std::clamp(cfg.client.backgroundAlpha, 0.0f, 1.0f) * mNativeFade;
        float const radius = std::max(0.0f, std::round(3.0f * fontScale));
        // A rounded outline is the outline colour drawn as a rounded rectangle with the panel
        // fill laid over it one unit in - there is no rounded stroke to draw.
        roundedFill(boxL, boxT, boxR, boxB, mce::Color{1.0f, 1.0f, 1.0f, 1.0f}, 0.3f * alpha, radius);
        roundedFill(
            boxL + 1.0f,
            boxT + 1.0f,
            boxR - 1.0f,
            boxB - 1.0f,
            mce::Color{0.0f, 0.0f, 0.0f, 1.0f},
            alpha,
            std::max(0.0f, radius - 1.0f)
        );
    }

    // The icon, drawn by the game's item renderer in these same units. Lamium flushes the
    // batched rectangles first; without it the item comes out with the UI fill material bound.
    if (auto* itemRenderer = client.getItemRenderer(); itemRenderer != nullptr && hasIcon) {
        BaseActorRenderContext renderContext(context.mScreenContext, client, client.getMinecraftGame_DEPRECATED());
        context.flushImages(mce::Color{1.0f, 1.0f, 1.0f, 1.0f}, 1.0f, HashedString{"ui_fillColor"});
        auto* const holder = client.getLocalPlayer();
        (void)holder;
        // Frame 0, like Lamium's target card (InfoHud.cpp:202/289). The animation frame only
        // means something for animated sprites (clock, compass); for the block branch it feeds
        // the model's pose, so passing a per-frame value made every 3D icon jitter while 2D
        // item icons stayed still.
        int const frame = 0;
        // The icon sits at the top of the cell, level with the first text line, rather than
        // centred on the whole panel: a tall panel (many extra lines) left it floating in the
        // middle, away from the name it belongs to.
        itemRenderer->renderGuiItemNew(
            renderContext,
            mIconStack,
            frame,
            boxL + padX,
            boxT + padY,
            false,
            mNativeFade, // the panel's fade: the icon used to pop in at full opacity
            1.0f,
            std::max(0.25f, iconSize / 16.0f),
            17
        );
    }

    // Text: one drawText per line inside the reserved area, flushed together, the way vanilla
    // draws its labels. Follows the morphing box, like the icon above.
    float y = boxT + padY;
    for (size_t i = 0; i < lines.size(); ++i) {
        RectangleArea const area{
            boxL + padX + iconCol,
            boxL + padX + iconCol + widest + 1.0f,
            y,
            y + lineStep
        };
        context.drawText(
            font,
            area,
            std::string{lines[i]},
            textColor,
            mNativeFade,
            ::ui::TextAlignment::Left,
            textData,
            caretData
        );
        y += lineStep;
    }
    context.flushText(0.0f, std::nullopt);
}

} // namespace insight
