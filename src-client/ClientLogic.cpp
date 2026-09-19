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
    // section signs (0xA7) inserted by colorizeAmpersand(). Matching the raw
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
        auto const& owner   = *screenContext.mOwner;
        auto const* pos     = std::get_if<BlockPos>(&owner);
        auto const* ownerId = std::get_if<ActorUniqueID>(&owner);
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
            bool preferred = key == "container_items";
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
        if (!pos && ownerId && bestFilled >= 0) {
            // Container entities (chest/hopper minecart, boat with chest) are
            // keyed by their unique id instead of a block position.
            setLiveEntityContainerSnapshot(
                static_cast<int64_t>(ownerId->rawID),
                bestFilled,
                bestSize,
                bestTotal
            );
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
    // /insight on the client: status / reload / set <option> <value> / gui
    mListeners.emplace_back(bus.emplaceListener<ll::event::command::ClientCommandRegisterEvent>([](auto&) {
        registerInsightCommand(true, nullptr, [] { ConfigUi::instance().setVisible(true); });
    }));

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
    content.visible         = wantVisible;
    content.anchor          = cfg.client.anchor;
    content.offsetX         = cfg.client.offsetX;
    content.offsetY         = cfg.client.offsetY;
    content.fontSize        = std::max(0.25f, cfg.client.fontSize);
    content.background      = cfg.client.background;
    content.backgroundAlpha = cfg.client.backgroundAlpha;
    content.shadow          = cfg.client.shadow;
    content.maxWidth        = cfg.client.maxWidth;

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
            if (entity && entityDist > (hit ? hit->distance : 1e30)) {
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
            {
                auto const& epos = entity->getPosition();
                BlockPos    eblock(
                    static_cast<int>(std::floor(epos.x)),
                    static_cast<int>(std::floor(epos.y)),
                    static_cast<int>(std::floor(epos.z))
                );
                info.light    = describeBlockLight(region, eblock);
                info.emission = describeBlockEmission(region, eblock);
            }
            text                          = renderText(cfg, info);
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
            text           = renderText(cfg, info);
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
            LookInfo info;
            text = renderText(cfg, info);
        }
        mText    = text;
        mVisible = !text.empty();
    }

    pushOverlay();
}

} // namespace insight
