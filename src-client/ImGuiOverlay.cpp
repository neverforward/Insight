// ImGui HUD overlay for the Insight client panel.
//
// Renders through Dear ImGui on a DXGI Present hook because the vanilla UI
// text pipeline (MinecraftUIRenderContext::drawText) cannot draw CJK names
// on this GDK build — only isolated glyphs ever showed. ImGui loads
// Microsoft YaHei and draws the multi-line colored panel with full control
// over geometry, background and shadow.
//
// Pure display overlay: no ImGui input backend, no WndProc subclassing, no
// input is swallowed. Payload is pushed on the game thread and snapshotted
// on the render thread inside Present.

#include "ImGuiOverlay.h"

#include <Windows.h>

// RenderDragon ships a very recent d3d12.h that renames legacy structs;
// follow the workaround used by other client mods so the SDK headers keep
// compiling against the Windows SDK d3d12 definitions.
#define D3D12_FEATURE_DATA_D3D12_OPTIONS D3D12_FEATURE_DATA_D3D12_OPTIONS_LEGACY
#define D3D12_FEATURE_DATA_ARCHITECTURE  D3D12_FEATURE_DATA_ARCHITECTURE_LEGACY
#define D3D12_RAYTRACING_GEOMETRY_DESC   D3D12_RAYTRACING_GEOMETRY_DESC_LEGACY
#include <d3d11.h>
#include <d3d11on12.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#undef D3D12_FEATURE_DATA_D3D12_OPTIONS
#undef D3D12_FEATURE_DATA_ARCHITECTURE
#undef D3D12_RAYTRACING_GEOMETRY_DESC

#include <MinHook.h>
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_win32.h>

// imgui_impl_win32.h intentionally keeps this declaration inside an '#if 0'
// block (it must not drag in <windows.h>); the backend asks applications to
// forward declare it themselves.
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#include <imgui.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <mutex>

#include "Insight.h"

namespace insight {

namespace {

using PresentFn       = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);
using Present1Fn      = HRESULT(__stdcall*)(IDXGISwapChain1*, UINT, UINT, DXGI_PRESENT_PARAMETERS const*);
using ResizeBuffersFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
using ResizeBuffers1Fn =
    HRESULT(__stdcall*)(IDXGISwapChain3*, UINT, UINT, UINT, DXGI_FORMAT, UINT, UINT const*, IUnknown* const*);
using ExecuteCommandListsFn = void(__stdcall*)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);

PresentFn             gOriginalPresent{};
Present1Fn            gOriginalPresent1{};
ResizeBuffersFn       gOriginalResizeBuffers{};
ResizeBuffers1Fn      gOriginalResizeBuffers1{};
ExecuteCommandListsFn gOriginalExecuteCommandLists{};

void* gPresentTarget{};
void* gPresent1Target{};
void* gResizeTarget{};
void* gResize1Target{};
void* gExecuteTarget{};

ID3D11Device*        gDevice{};
ID3D11DeviceContext* gDeviceContext{};
ID3D11On12Device*    gDevice11On12{};
ID3D12CommandQueue*  gGameQueue{};
IDXGISwapChain*      gActiveSwapChain{};

std::atomic_bool   gInstalled{false};
std::atomic_bool   gShuttingDown{false};
std::atomic_bool   gRendering{false};
std::atomic_ullong gGraphicsResumeAt{};
std::mutex         gResourceMutex;
bool               gImGuiInitialized{};
bool               gGraphicsInitialized{};

ImFont* gPanelFont{}; // CJK-capable font used for the panel text
float   gPanelFontBaseSize = 22.0f;

// Font handling: one CJK font with the full Chinese glyph range (see
// loadFonts()), drawn at gPanelFontBaseSize. The raster size is kept at a
// value whose atlas this D3D11On12 device can actually create.
constexpr float gFontRaster = 24.0f; // atlas raster px

// Frame payload (last value wins; read once per Present).
std::mutex            gContentMutex;
ImGuiOverlay::Content gContent;
// Panel transition state (render thread only):
//   gPanelFade    - 0..1 fade of the whole panel while it is switched on or off.
//   gMorphT       - 0..1 progress of the box resize after the subject changed.
//   gShownContent - the snapshot that is on screen. Kept because the game thread
//                   stops filling the lines as soon as the display is switched
//                   off, so a fade-out would otherwise have nothing to draw.
float                 gPanelFade = 0.0f;
float                 gMorphT    = 1.0f;
ImGuiOverlay::Content gShownContent;
std::string           gShownKey;
// Geometry of the panel as it was drawn last: the size transition morphs towards the
// new text's box from here instead of snapping it into place within one frame.
float gBoxW  = 0.0f;
float gBoxH  = 0.0f;
float gFromW = 0.0f;
float gFromH = 0.0f;
// extra window (the configuration screen) drawn inside the same frame; the
// setter runs on the game thread, the reader on the render thread
std::mutex            gDrawerMutex;
std::function<void()> gWindowDrawer;

// ImGui's Win32 backend owns mouse/keyboard/wheel input (the engine's mouse
// events are not usable as cursor input on this client), so it takes over the
// game's window procedure. While the configuration screen is open the mouse and
// key messages are swallowed so the game does not react to them.
HWND              gGameWindow{};
WNDPROC           gOriginalWndProc{};
bool              gWin32BackendReady = false;
std::atomic<bool> gInputCaptured{false};

// Cursor handoff for the configuration screen.
//
// The display counter that ShowCursor() drives is *thread* state, and the screen
// is closed on the render thread, so calling ShowCursor there changes the wrong
// counter and the pointer stays on screen. Both halves therefore run on the
// window thread, posted as messages and handled in windowProc() below. Only the
// increments this mod forced are ever given back, so Minecraft's own negative
// count (the one that hides the pointer in gameplay) is left untouched.
constexpr UINT   kMsgAcquireMenuCursor   = WM_APP + 0x11;
constexpr UINT   kMsgRestoreNativeCursor = WM_APP + 0x12;
std::atomic_int  gMenuCursorShowCount{};
std::atomic_bool gResetImGuiMouse{false};
RECT             gSavedClip{};
bool             gClipRestore = false;
bool             gClipOwned   = false;

void acquireMenuCursor() {
    // ShowCursor() returns the counter *after* the call: a value above 0 means the
    // pointer was already visible and this probe has to be undone. That keeps the
    // per-frame re-assertions idempotent instead of drifting upwards.
    if (::ShowCursor(TRUE) > 0) {
        ::ShowCursor(FALSE);
        return;
    }
    gMenuCursorShowCount.fetch_add(1, std::memory_order_relaxed);
}

void releaseMenuCursor() {
    while (gMenuCursorShowCount.load(std::memory_order_relaxed) > 0) {
        gMenuCursorShowCount.fetch_sub(1, std::memory_order_relaxed);
        ::ShowCursor(FALSE);
    }
}

// Hand the pointer back where the game expects it: locked inside its window and
// centred, so the first relative-look frame after closing the screen does not
// jump by the distance the pointer travelled over the menu.
void restoreGameCursorClip(HWND window) {
    ::ClipCursor(nullptr);
    RECT clip{};
    bool haveClip = false;
    if (gClipOwned && gClipRestore) {
        clip     = gSavedClip; // whatever the game had installed itself
        haveClip = true;
    } else {
        RECT  client{};
        POINT topLeft{};
        POINT bottomRight{};
        if (window && ::GetClientRect(window, &client)) {
            topLeft     = POINT{client.left, client.top};
            bottomRight = POINT{client.right, client.bottom};
            if (::ClientToScreen(window, &topLeft) && ::ClientToScreen(window, &bottomRight)) {
                clip     = RECT{topLeft.x, topLeft.y, bottomRight.x, bottomRight.y};
                haveClip = true;
            }
        }
    }
    if (haveClip && ::ClipCursor(&clip)) {
        ::SetCursorPos((clip.left + clip.right) / 2, (clip.top + clip.bottom) / 2);
    }
    gClipOwned = false;
}

LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (gWin32BackendReady) {
        ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam);
    }
    switch (message) {
    case kMsgAcquireMenuCursor:
        if (!gClipOwned) {
            gClipRestore = ::GetClipCursor(&gSavedClip) != FALSE;
            gClipOwned   = true;
        }
        ::ClipCursor(nullptr);
        acquireMenuCursor();
        return 0;
    case kMsgRestoreNativeCursor:
        releaseMenuCursor();
        // Give the standard arrow back instead of a null cursor: hiding is the
        // display counter's job (Minecraft's own negative count is restored
        // above), while a null *image* would stick and leave every later screen
        // without a pointer - a Minecraft menu only raises the counter, it does
        // not necessarily set an image of its own.
        ::SetCursor(::LoadCursorW(nullptr, IDC_ARROW));
        restoreGameCursorClip(window);
        return 0;
    default:
        break;
    }
    if (message == WM_KILLFOCUS || (message == WM_ACTIVATEAPP && wParam == FALSE)) {
        // never keep the pointer while another window is in front
        releaseMenuCursor();
        ::ClipCursor(nullptr);
        gClipOwned = false;
    }
    if (gInputCaptured.load(std::memory_order_acquire)) {
        switch (message) {
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
        case WM_XBUTTONDOWN:
        case WM_XBUTTONUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_CHAR:
        case WM_SETCURSOR: // ImGui sets the shape itself while it owns the mouse
            return 0;
        case WM_INPUT:
        case WM_INPUT_DEVICE_CHANGE:
            // raw input must still reach DefWindowProc for user32's bookkeeping,
            // but the game must not look around while the screen is open
            return ::DefWindowProcW(window, message, wParam, lParam);
        default:
            break;
        }
    }
    return gOriginalWndProc ? CallWindowProcW(gOriginalWndProc, window, message, wParam, lParam) : 0;
}
std::chrono::steady_clock::time_point gLastFrameTime{};

constexpr size_t kPresentVtableIndex             = 8;
constexpr size_t kResizeBuffersVtableIndex       = 13;
constexpr size_t kPresent1VtableIndex            = 22;
constexpr size_t kResizeBuffers1VtableIndex      = 39;
constexpr size_t kExecuteCommandListsVtableIndex = 10;

constexpr ULONGLONG kResizeGraphicsResumeDelayMs = 100;

auto& logger() { return Insight::getInstance().getSelf().getLogger(); }

[[nodiscard]] HWND findProcessWindow() {
    struct Search {
        DWORD pid;
        HWND  result;
    } search{GetCurrentProcessId(), nullptr};
    EnumWindows(
        [](HWND window, LPARAM parameter) -> BOOL {
            auto& state = *reinterpret_cast<Search*>(parameter);
            DWORD pid{};
            GetWindowThreadProcessId(window, &pid);
            if (pid == state.pid && IsWindowVisible(window) && GetWindow(window, GW_OWNER) == nullptr) {
                state.result = window;
                return FALSE;
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&search)
    );
    return search.result;
}

void logGraphicsFailure(IDXGISwapChain* swapChain, char const* operation, HRESULT result) {
    ID3D12Device* device12{};
    HRESULT       removed = S_OK;
    if (swapChain && SUCCEEDED(swapChain->GetDevice(__uuidof(ID3D12Device), reinterpret_cast<void**>(&device12)))) {
        removed = device12->GetDeviceRemovedReason();
        device12->Release();
    }
    logger().error(
        "ImGui graphics call {} failed: HRESULT=0x{:08X}, deviceRemovedReason=0x{:08X}",
        operation,
        static_cast<unsigned int>(result),
        static_cast<unsigned int>(removed)
    );
}

void releaseGraphicsBackend() {
    if (gGraphicsInitialized) {
        if (gWin32BackendReady) {
            ImGuiOverlay::setInputCaptured(false); // also restores clip + cursor
            if (gOriginalWndProc && gGameWindow && IsWindow(gGameWindow)) {
                SetWindowLongPtrW(gGameWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(gOriginalWndProc));
            }
            gOriginalWndProc   = nullptr;
            gGameWindow        = nullptr;
            gWin32BackendReady = false;
            ImGui_ImplWin32_Shutdown();
        }
        ImGui_ImplDX11_Shutdown();
        gGraphicsInitialized = false;
    }
    if (gDeviceContext) {
        ID3D11RenderTargetView* empty{};
        gDeviceContext->OMSetRenderTargets(1, &empty, nullptr);
        gDeviceContext->ClearState();
        gDeviceContext->Flush();
    }
    if (gDevice11On12) {
        gDevice11On12->Release();
        gDevice11On12 = nullptr;
    }
    if (gDeviceContext) {
        gDeviceContext->Release();
        gDeviceContext = nullptr;
    }
    if (gDevice) {
        gDevice->Release();
        gDevice = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Font setup, following the approach used by the reference client mods
// (LHolo / playback): one CJK font loaded from the system with the full
// Chinese glyph range, an optional icon-font merge, and io.FontDefault set so
// the UI font is the CJK one. The raster size stays at the value that is
// known to produce a font texture this D3D11On12 device can create (a larger
// atlas made CreateTexture2D fail earlier).
// ---------------------------------------------------------------------------
std::string resolveFontPath() {
    auto const                         modDir     = Insight::getInstance().getSelf().getModDir();
    std::vector<std::filesystem::path> candidates = {
        modDir / "MiSans-Regular.ttf",
        modDir / "MiSans.ttf",
        modDir / "font.ttf",
        modDir / "MiSansVF.ttf",
        "C:\\Windows\\Fonts\\MiSans-Regular.ttf",
        "C:\\Windows\\Fonts\\MiSans.ttf",
        "C:\\Windows\\Fonts\\msyh.ttc",
    };
    for (auto const& c : candidates) {
        if (std::filesystem::exists(c)) {
            return c.string();
        }
    }
    return {};
}

// Load the panel font into the current ImGui context's atlas.
void loadFonts() {
    auto& io = ImGui::GetIO();

    static std::string const fontPath = resolveFontPath();
    if (fontPath.empty()) {
        logger().error("ImGui: no CJK font found, falling back to the default font");
        gPanelFont     = io.Fonts->AddFontDefault();
        io.FontDefault = gPanelFont;
        return;
    }

    ImFontConfig config{};
    config.OversampleH = 2;
    config.OversampleV = 2;
    ImFont* font =
        io.Fonts->AddFontFromFileTTF(fontPath.c_str(), gFontRaster, &config, io.Fonts->GetGlyphRangesChineseFull());
    if (!font) {
        logger().error("ImGui: failed to load '{}', falling back to the default font", fontPath);
        font = io.Fonts->AddFontDefault();
    } else {
        logger().info("ImGui overlay font: {} ({}px, full CJK range)", fontPath, gFontRaster);
    }

    // Merge a couple of symbols the CJK range does not contain (e.g. the
    // warning sign used by vanilla text) from Windows' symbol font.
    static char const* symbolFont = "C:\\Windows\\Fonts\\seguisym.ttf";
    if (std::filesystem::exists(symbolFont)) {
        static constexpr ImWchar symbolRange[]{0x26A0, 0x26A0, 0};
        ImFontConfig             symbolConfig{};
        symbolConfig.MergeMode   = true;
        symbolConfig.PixelSnapH  = true;
        symbolConfig.OversampleH = 2;
        symbolConfig.OversampleV = 2;
        io.Fonts->AddFontFromFileTTF(symbolFont, gFontRaster, &symbolConfig, symbolRange);
    }

    io.FontDefault = font;
    gPanelFont     = font;
}


bool initializeImGui(IDXGISwapChain* swapChain) {
    if (gImGuiInitialized && gGraphicsInitialized) {
        return true;
    }
    if (GetTickCount64() < gGraphicsResumeAt.load(std::memory_order_acquire)) {
        return false;
    }

    releaseGraphicsBackend();

    if (SUCCEEDED(swapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&gDevice)))) {
        gDevice->GetImmediateContext(&gDeviceContext);
    } else {
        // RenderDragon usually exposes a D3D12 device; bridge it with
        // D3D11On12 so the ImGui DX11 backend can render on the game queue.
        if (!gGameQueue) {
            return false;
        }
        ID3D12Device* device12{};
        if (FAILED(swapChain->GetDevice(__uuidof(ID3D12Device), reinterpret_cast<void**>(&device12)))) {
            return false;
        }
        auto result = D3D11On12CreateDevice(
            device12,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            nullptr,
            0,
            reinterpret_cast<IUnknown**>(&gGameQueue),
            1,
            0,
            &gDevice,
            &gDeviceContext,
            nullptr
        );
        device12->Release();
        if (FAILED(result) || !gDevice) {
            releaseGraphicsBackend();
            return false;
        }
        if (FAILED(gDevice->QueryInterface(__uuidof(ID3D11On12Device), reinterpret_cast<void**>(&gDevice11On12)))) {
            releaseGraphicsBackend();
            return false;
        }
    }

    if (!gImGuiInitialized) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        auto& ioc       = ImGui::GetIO();
        ioc.IniFilename = nullptr;
        ioc.LogFilename = nullptr;
        // keyboard navigation, so the configuration screen is usable without a
        // mouse (arrow keys + Enter); the mouse itself arrives through the Win32
        // backend, which is installed further down in this function
        ioc.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        // during play the game owns the cursor: ImGui must not touch it, or its
        // backend keeps forcing a visible arrow back over the game's hidden
        // pointer. setInputCaptured() lifts this while the screen is open.
        ioc.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
        ImGui::StyleColorsDark();
        loadFonts();
        if (!ioc.Fonts->Build()) {
            logger().error("ImGui: font atlas build failed");
        }
        logger().info("ImGui font atlas: {}x{}px", ioc.Fonts->TexWidth, ioc.Fonts->TexHeight);
        gImGuiInitialized = true;
        logger().info("Injected Dear ImGui overlay initialized");
    }
    // Win32 backend: install our window procedure on the game window as soon as
    // the swap chain (and with it the HWND) is known.
    if (!gWin32BackendReady) {
        DXGI_SWAP_CHAIN_DESC scDesc{};
        HWND                 hwnd{};
        if (SUCCEEDED(swapChain->GetDesc(&scDesc))) {
            hwnd = scDesc.OutputWindow;
        }
        if (hwnd && ImGui_ImplWin32_Init(hwnd)) {
            gGameWindow      = hwnd;
            gOriginalWndProc = reinterpret_cast<WNDPROC>(
                SetWindowLongPtrW(gGameWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(windowProc))
            );
            gWin32BackendReady = true;
        }
    }
    if (!ImGui_ImplDX11_Init(gDevice, gDeviceContext)) {
        releaseGraphicsBackend();
        return false;
    }
    if (!gPanelFont) {
        gPanelFont = ImGui::GetIO().Fonts->Fonts.empty() ? nullptr : ImGui::GetIO().Fonts->Fonts[0];
    }

    // Create every D3D11 device object (shaders, states, the font texture)
    // here, *outside* the wrapped-resource Acquire critical section inside
    // render(). Creating them lazily on the first RenderDrawData crashed with
    // a null dereference inside CreateDeviceObjects on this D3D11On12 game.
    if (!ImGui_ImplDX11_CreateDeviceObjects()) {
        logger().error("ImGui: ImGui_ImplDX11_CreateDeviceObjects failed");
        releaseGraphicsBackend();
        return false;
    }
    gDeviceContext->Flush(); // push the font-texture upload through 11on12

    gGraphicsInitialized = true;
    gActiveSwapChain     = swapChain;
    logger().info("ImGui graphics backend initialized (font atlas {})", gPanelFont ? "ok" : "missing");
    return true;
}

[[nodiscard]] std::uint32_t toImU32(float r, float g, float b, float a = 1.0f) {
    auto c = [](float v) -> int {
        int x = static_cast<int>(v * 255.0f + 0.5f);
        return x < 0 ? 0 : x > 255 ? 255 : x;
    };
    return IM_COL32(c(r), c(g), c(b), c(a));
}

// Panel drawing. `io.DisplaySize` and `io.DeltaTime` are expected to be set
// before this runs (between NewFrame calls).
// Anchor point of the panel inside a w x h area, including the configured
// offsets. A positive offset always pushes the panel away from the edge it is
// anchored at, towards the middle of the area (for centre anchors: right/down),
// so the whole 0..1 slider range stays usable with every anchor instead of only
// the half that happens to point inwards.
void anchorOrigin(
    std::string const& anchor,
    float              offsetX,
    float              offsetY,
    float              w,
    float              h,
    float&             u,
    float&             v,
    float&             cx,
    float&             cy
) {
    u = 0.5f;
    v = 0.5f;
    if (anchor.find("left") != std::string::npos) {
        u = 0.0f;
    } else if (anchor.find("right") != std::string::npos) {
        u = 1.0f;
    }
    if (anchor.find("top") != std::string::npos) {
        v = 0.0f;
    } else if (anchor.find("bottom") != std::string::npos) {
        v = 1.0f;
    }
    float const sx = u < 0.34f ? 1.0f : (u > 0.66f ? -1.0f : 1.0f);
    float const sy = v < 0.34f ? 1.0f : (v > 0.66f ? -1.0f : 1.0f);
    cx             = u * w + offsetX * sx * w;
    cy             = v * h + offsetY * sy * h;
}

// How the panel is drawn for one frame. There is only ever one panel: it fades in
// and out with the display, and its box is resized when the subject changes - the
// lines themselves are swapped at once, because fading them made every change of the
// subject look like a flash.
struct PanelDraw {
    float fade       = 1.0f; // alpha of the whole panel (text and box together)
    float morphFromW = 0.0f; // box size to resize from (0 = the natural size)
    float morphFromH = 0.0f;
    float morphT     = 1.0f; // resize progress
};

void drawPanel(ImGuiOverlay::Content const& content, PanelDraw const& opts) {
    float const fade = std::clamp(opts.fade, 0.0f, 1.0f);
    auto&       io   = ImGui::GetIO();
    if (io.DisplaySize.x <= 0.0f || io.DisplaySize.y <= 0.0f) {
        return;
    }
    if (fade <= 0.0f) {
        return;
    }
    float w = io.DisplaySize.x;
    float h = io.DisplaySize.y;

    ImFont* font = gPanelFont ? gPanelFont : ImGui::GetFont();
    if (!font) {
        return;
    }

    float fontSizePx = std::max(8.0f, gPanelFontBaseSize * content.fontSize);
    float lineStep   = std::ceil(fontSizePx * 1.35f);
    float padX       = 8.0f;
    float padY       = 5.0f;

    // Width limit (client.maxWidth, fraction of the screen): split lines into
    // visual lines at character boundaries so text never overflows. The panel is
    // never allowed to grow past the display either - a wider box would be pinned
    // to the left screen edge and the horizontal offset could not move it.
    std::vector<std::vector<ImGuiOverlay::Run>> displayLines;
    float                                       maxTextWidth = std::max(24.0f, w - 2.0f * padX - 4.0f);
    if (content.maxWidth > 0.0f) {
        maxTextWidth = std::min(maxTextWidth, std::max(24.0f, content.maxWidth * w - 2.0f * padX));
    }
    if (maxTextWidth <= 0.0f) {
        displayLines = content.lines;
    } else {
        for (auto const& line : content.lines) {
            std::vector<ImGuiOverlay::Run> current;
            float                          currentWidth = 0.0f;
            auto                           pushChar     = [&](ImGuiOverlay::Run const& run, std::string const& ch) {
                float w1 = font->CalcTextSizeA(fontSizePx, FLT_MAX, 0.0f, ch.c_str()).x;
                if (currentWidth + w1 > maxTextWidth && !current.empty()) {
                    displayLines.push_back(current);
                    current.clear();
                    currentWidth = 0.0f;
                }
                if (!current.empty() && current.back().r == run.r && current.back().g == run.g
                    && current.back().b == run.b) {
                    current.back().text += ch;
                } else {
                    current.push_back(ImGuiOverlay::Run{ch, run.r, run.g, run.b});
                }
                currentWidth += w1;
            };
            for (auto const& run : line) {
                size_t i = 0;
                while (i < run.text.size()) {
                    auto   b   = static_cast<unsigned char>(run.text[i]);
                    size_t len = 1;
                    if ((b & 0xE0) == 0xC0) {
                        len = 2;
                    } else if ((b & 0xF0) == 0xE0) {
                        len = 3;
                    } else if ((b & 0xF8) == 0xF0) {
                        len = 4;
                    }
                    if (i + len > run.text.size()) {
                        len = run.text.size() - i;
                    }
                    pushChar(run, run.text.substr(i, len));
                    i += len;
                }
            }
            displayLines.push_back(current);
        }
    }

    // Measure every color run with the same font/size used for drawing.
    std::vector<std::vector<float>> lineWidths(displayLines.size());
    std::vector<float>              totalWidths(displayLines.size(), 0.0f);
    float                           maxLineWidth = 0.0f;
    for (size_t i = 0; i < displayLines.size(); ++i) {
        auto& widths = lineWidths[i];
        widths.reserve(displayLines[i].size());
        for (auto const& run : displayLines[i]) {
            auto size = font->CalcTextSizeA(fontSizePx, FLT_MAX, 0.0f, run.text.c_str());
            widths.push_back(size.x);
            totalWidths[i] += size.x;
        }
        maxLineWidth = std::max(maxLineWidth, totalWidths[i]);
    }

    float boxW = maxLineWidth + 2.0f * padX;
    float boxH = static_cast<float>(displayLines.size()) * lineStep + 2.0f * padY;

    // Size transition: grow or shrink from the box that was on screen when the
    // subject changed instead of snapping to the new text's size. `morphT` is the
    // same progress that crossfades the text, so the two stay in step.
    if (opts.morphT < 1.0f && opts.morphFromW > 0.0f && opts.morphFromH > 0.0f) {
        boxW = opts.morphFromW + (boxW - opts.morphFromW) * opts.morphT;
        boxH = opts.morphFromH + (boxH - opts.morphFromH) * opts.morphT;
    }
    gBoxW = boxW;
    gBoxH = boxH;

    // Anchor point of the panel on screen plus the configured offsets.
    float u  = 0.5f;
    float v  = 0.5f;
    float cx = 0.0f;
    float cy = 0.0f;
    anchorOrigin(content.anchor, content.offsetX, content.offsetY, w, h, u, v, cx, cy);

    // Vertical extent grows inward from top/bottom anchors.
    float boxTop    = 0.0f;
    float boxBottom = 0.0f;
    if (v <= 0.001f) {
        boxTop    = cy;
        boxBottom = cy + boxH;
    } else if (v >= 0.999f) {
        boxTop    = cy - boxH;
        boxBottom = cy;
    } else {
        boxTop    = cy - boxH / 2.0f;
        boxBottom = cy + boxH / 2.0f;
    }
    float boxLeft  = 0.0f;
    float boxRight = 0.0f;
    if (u <= 0.001f) {
        boxLeft  = cx;
        boxRight = cx + boxW;
    } else if (u >= 0.999f) {
        boxLeft  = cx - boxW;
        boxRight = cx;
    } else {
        boxLeft  = cx - boxW / 2.0f;
        boxRight = cx + boxW / 2.0f;
    }
    // Keep the whole panel inside the screen.
    boxLeft   = std::clamp(boxLeft, 2.0f, std::max(2.0f, w - boxW - 2.0f));
    boxRight  = boxLeft + boxW;
    boxTop    = std::clamp(boxTop, 2.0f, std::max(2.0f, h - boxH - 2.0f));
    boxBottom = boxTop + boxH;

    auto* draw = ImGui::GetForegroundDrawList();

    if (content.background) {
        float alpha = std::clamp(content.backgroundAlpha, 0.0f, 1.0f) * fade;
        draw->AddRectFilled(ImVec2(boxLeft, boxTop), ImVec2(boxRight, boxBottom), toImU32(0.0f, 0.0f, 0.0f, alpha));
        draw->AddRect(
            ImVec2(boxLeft, boxTop),
            ImVec2(boxRight, boxBottom),
            toImU32(1.0f, 1.0f, 1.0f, 0.3f * alpha),
            0.0f,
            0,
            1.0f
        );
    }

    // Horizontal alignment inside the panel.
    bool left  = u < 0.34f;
    bool right = u > 0.66f;

    float yCursor = boxTop + padY;
    for (size_t i = 0; i < displayLines.size(); ++i) {
        float xStart = boxLeft + padX;
        if (!left && !right) {
            xStart = (boxLeft + boxRight) / 2.0f - totalWidths[i] / 2.0f;
        } else if (right) {
            xStart = boxRight - padX - totalWidths[i];
        }
        float xCursor = xStart;
        for (size_t j = 0; j < displayLines[i].size(); ++j) {
            auto const& run  = displayLines[i][j];
            auto        col  = toImU32(run.r, run.g, run.b, fade);
            auto        colS = toImU32(0.0f, 0.0f, 0.0f, (content.background ? 0.85f : 0.5f) * fade);
            ImVec2      pos(xCursor, yCursor);
            if (content.shadow) {
                draw->AddText(font, fontSizePx, ImVec2(pos.x + 1.0f, pos.y + 1.0f), colS, run.text.c_str());
            }
            draw->AddText(font, fontSizePx, pos, col, run.text.c_str());
            xCursor += lineWidths[i][j];
        }
        yCursor += lineStep;
    }
}

void render(IDXGISwapChain* swapChain) {
    if (gShuttingDown.load(std::memory_order_acquire)) {
        return;
    }
    if (gRendering.exchange(true, std::memory_order_acq_rel)) {
        return;
    }
    struct Reset {
        ~Reset() { gRendering.store(false, std::memory_order_release); }
    } reset;

    std::lock_guard lock(gResourceMutex);
    if (gActiveSwapChain && gActiveSwapChain != swapChain) {
        return; // another chain (composition etc.) — never claim it
    }
    if (!initializeImGui(swapChain)) {
        return;
    }

    ImGuiOverlay::Content content;
    {
        std::lock_guard cLock(gContentMutex);
        content = gContent;
    }
    std::function<void()> drawer;
    {
        std::lock_guard dLock(gDrawerMutex);
        drawer = gWindowDrawer;
    }
    bool hasText = false;
    for (auto const& line : content.lines) {
        if (!line.empty()) {
            hasText = true;
            break;
        }
    }
    bool const wantVisible = content.visible && hasText;
    // The frame runs when there is something to draw: the HUD text, a panel that
    // is still fading out, or the configuration window (which keeps drawing with
    // the HUD hidden).
    if (!wantVisible && gPanelFade <= 0.0f && !drawer) {
        return;
    }

    // Display metrics for ImGui (no Win32 backend installed).
    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(swapChain->GetDesc(&desc)) || desc.BufferDesc.Width == 0 || desc.BufferDesc.Height == 0) {
        return;
    }
    auto& io       = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(desc.BufferDesc.Width), static_cast<float>(desc.BufferDesc.Height));
    auto now       = std::chrono::steady_clock::now();
    io.DeltaTime =
        gLastFrameTime.time_since_epoch().count() == 0
            ? (1.0f / 60.0f)
            : std::clamp(std::chrono::duration<float>(now - gLastFrameTime).count(), 1.0f / 240.0f, 1.0f / 10.0f);
    gLastFrameTime = now;

    // Panel transition. The step comes from the frame time, so a duration means the
    // same whatever the framerate is; 0 keeps the instantaneous behaviour (and is also
    // the fallback while ImGui has no frame time yet).
    float const duration = std::max(0.0f, content.transitionTime);
    float const step     = duration > 0.0f ? io.DeltaTime / duration : 1.0f;
    {
        // Appearing out of nothing: the panel fades in with whatever is under the
        // crosshair now and has no size to grow from.
        bool const appearing = wantVisible && gPanelFade <= 0.0f;
        if (appearing) {
            gShownContent = content;
            gShownKey     = content.targetKey;
            gFromW        = 0.0f;
            gFromH        = 0.0f;
            gMorphT       = 1.0f;
        } else if (wantVisible) {
            if (content.targetKey != gShownKey) {
                // Another block or entity. The text is swapped at once - fading it
                // made every change of the subject look like a flash - and only the
                // box is resized, from the size it had to the size the new lines need.
                gShownContent = content;
                gShownKey     = content.targetKey;
                gFromW        = gBoxW;
                gFromH        = gBoxH;
                gMorphT       = 0.0f;
            } else {
                // same subject, new text (a ticking timer, health, ...): follow it too
                gShownContent = content;
            }
        }
        gMorphT = std::min(1.0f, gMorphT + step);
        float const target = wantVisible ? 1.0f : 0.0f;
        gPanelFade = target > gPanelFade ? std::min(target, gPanelFade + step)
                                         : std::max(target, gPanelFade - step);
    }

    // Re-asserted every frame while the screen is open: a Minecraft screen
    // transition, an alt-tab or another overlay can hide the pointer behind our
    // back. The window handler is idempotent, so this cannot drift the counter.
    bool const menuOpen = gInputCaptured.load(std::memory_order_acquire);
    if (menuOpen && gGameWindow && ::IsWindow(gGameWindow)) {
        ::PostMessageW(gGameWindow, kMsgAcquireMenuCursor, 0, 0);
    }
    // the menu draws with the native cursor, never ImGui's software one
    ImGui::GetIO().MouseDrawCursor = false;

    auto draw = [&](ID3D11RenderTargetView* target) {
        gDeviceContext->OMSetRenderTargets(1, &target, nullptr);
        if (gResetImGuiMouse.exchange(false, std::memory_order_acq_rel)) {
            // ImGui tracks the buttons separately from Win32, so the state of the
            // click that closed the screen has to be dropped before the game gets
            // the mouse back
            auto& io = ImGui::GetIO();
            for (bool& down : io.MouseDown) {
                down = false;
            }
            io.MouseWheel  = 0.0f;
            io.MouseWheelH = 0.0f;
        }
        ImGui_ImplWin32_NewFrame();
        ImGui_ImplDX11_NewFrame();
        ImGui::NewFrame();
        // The configuration screen is modal and shows the panel as its own
        // preview, so the real one is not drawn underneath it as well.
        if (!menuOpen && (wantVisible || gPanelFade > 0.0f)) {
            // One panel, and only the transition the user asked for: it fades in and
            // out with the display, and when the subject changes its box is resized
            // from the size it had. Text and box keep the panel's own alpha, so a
            // change of the subject never flashes.
            PanelDraw panel;
            panel.fade       = gPanelFade;
            panel.morphFromW = gFromW;
            panel.morphFromH = gFromH;
            panel.morphT     = wantVisible ? gMorphT : 1.0f;
            drawPanel(gShownContent, panel);
        }
        if (drawer) {
            drawer(); // configuration screen, drawn on top of the HUD
        }
        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        ID3D11RenderTargetView* empty{};
        gDeviceContext->OMSetRenderTargets(1, &empty, nullptr);
    };

    if (gDevice11On12) {
        UINT             index = 0;
        IDXGISwapChain3* swapChain3{};
        if (SUCCEEDED(swapChain->QueryInterface(__uuidof(IDXGISwapChain3), reinterpret_cast<void**>(&swapChain3)))) {
            index = swapChain3->GetCurrentBackBufferIndex();
            swapChain3->Release();
        }
        ID3D12Resource* backBuffer{};
        if (FAILED(swapChain->GetBuffer(index, __uuidof(ID3D12Resource), reinterpret_cast<void**>(&backBuffer)))) {
            return;
        }
        ID3D11Resource*      wrapped{};
        D3D11_RESOURCE_FLAGS flags{D3D11_BIND_RENDER_TARGET};
        if (FAILED(gDevice11On12->CreateWrappedResource(
                backBuffer,
                &flags,
                D3D12_RESOURCE_STATE_PRESENT,
                D3D12_RESOURCE_STATE_PRESENT,
                __uuidof(ID3D11Resource),
                reinterpret_cast<void**>(&wrapped)
            ))) {
            backBuffer->Release();
            return;
        }
        backBuffer->Release();
        ID3D11RenderTargetView* target{};
        if (SUCCEEDED(gDevice->CreateRenderTargetView(wrapped, nullptr, &target)) && target) {
            gDevice11On12->AcquireWrappedResources(&wrapped, 1);
            draw(target);
            gDevice11On12->ReleaseWrappedResources(&wrapped, 1);
            gDeviceContext->Flush();
            target->Release();
        }
        wrapped->Release();
    } else {
        ID3D11Texture2D* backBuffer{};
        if (FAILED(swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer)))) {
            return;
        }
        ID3D11RenderTargetView* target{};
        if (SUCCEEDED(gDevice->CreateRenderTargetView(backBuffer, nullptr, &target)) && target) {
            draw(target);
            target->Release();
        }
        backBuffer->Release();
    }
}

HRESULT __stdcall presentHook(IDXGISwapChain* swapChain, UINT interval, UINT flags) {
    render(swapChain);
    return gOriginalPresent(swapChain, interval, flags);
}

HRESULT __stdcall
present1Hook(IDXGISwapChain1* swapChain, UINT interval, UINT flags, DXGI_PRESENT_PARAMETERS const* parameters) {
    render(swapChain);
    return gOriginalPresent1(swapChain, interval, flags, parameters);
}

void tearDownForResizeLocked() {
    if (gGraphicsInitialized) {
        releaseGraphicsBackend();
    }
    gGraphicsResumeAt.store(GetTickCount64() + kResizeGraphicsResumeDelayMs, std::memory_order_release);
}

HRESULT __stdcall
resizeHook(IDXGISwapChain* swapChain, UINT count, UINT width, UINT height, DXGI_FORMAT format, UINT flags) {
    std::unique_lock lock(gResourceMutex);
    if (gActiveSwapChain != swapChain) {
        lock.unlock();
        return gOriginalResizeBuffers(swapChain, count, width, height, format, flags);
    }
    tearDownForResizeLocked();
    auto result = gOriginalResizeBuffers(swapChain, count, width, height, format, flags);
    if (FAILED(result)) {
        logGraphicsFailure(swapChain, "ResizeBuffers", result);
    }
    return result;
}

HRESULT __stdcall resize1Hook(
    IDXGISwapChain3* swapChain,
    UINT             count,
    UINT             width,
    UINT             height,
    DXGI_FORMAT      format,
    UINT             flags,
    UINT const*      nodeMask,
    IUnknown* const* presentQueue
) {
    std::unique_lock lock(gResourceMutex);
    if (gActiveSwapChain != static_cast<IDXGISwapChain*>(swapChain)) {
        lock.unlock();
        return gOriginalResizeBuffers1(swapChain, count, width, height, format, flags, nodeMask, presentQueue);
    }
    tearDownForResizeLocked();
    auto result = gOriginalResizeBuffers1(swapChain, count, width, height, format, flags, nodeMask, presentQueue);
    if (FAILED(result)) {
        logGraphicsFailure(swapChain, "ResizeBuffers1", result);
    }
    return result;
}

void executeCommandListsHook(ID3D12CommandQueue* queue, UINT count, ID3D12CommandList* const* lists) {
    if (!gGameQueue && queue->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
        std::lock_guard lock(gResourceMutex);
        if (!gGameQueue) {
            gGameQueue = queue;
            gGameQueue->AddRef();
        }
    }
    gOriginalExecuteCommandLists(queue, count, lists);
}

bool installHook(void* target, void* detour, void** original) {
    return target && MH_CreateHook(target, detour, original) == MH_OK && MH_EnableHook(target) == MH_OK;
}

void removeHook(void*& target) {
    if (!target) {
        return;
    }
    MH_DisableHook(target);
    MH_RemoveHook(target);
    target = nullptr;
}

} // namespace

ImGuiOverlay::~ImGuiOverlay() { shutdown(); }

bool ImGuiOverlay::install() {
    if (gInstalled.load(std::memory_order_acquire)) {
        return true;
    }
    gShuttingDown.store(false, std::memory_order_release);

    auto window = findProcessWindow();
    if (!window) {
        logger().warn("ImGui overlay: no game window yet, hooks not installed");
        return false;
    }
    auto status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) {
        logger().error("ImGui overlay: MinHook init failed ({})", static_cast<int>(status));
        return false;
    }

    // Read the DXGI vtable slot addresses from a throwaway swap chain.
    D3D_FEATURE_LEVEL    featureLevel = D3D_FEATURE_LEVEL_11_0;
    DXGI_SWAP_CHAIN_DESC description{};
    description.BufferCount       = 1;
    description.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.BufferUsage       = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    description.OutputWindow      = window;
    description.SampleDesc.Count  = 1;
    description.Windowed          = TRUE;
    description.SwapEffect        = DXGI_SWAP_EFFECT_DISCARD;

    ID3D11Device*        dummyDevice{};
    ID3D11DeviceContext* dummyContext{};
    IDXGISwapChain*      dummySwapChain{};
    if (FAILED(D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            0,
            &featureLevel,
            1,
            D3D11_SDK_VERSION,
            &description,
            &dummySwapChain,
            &dummyDevice,
            nullptr,
            &dummyContext
        ))) {
        logger().error("ImGui overlay: cannot create dummy swap chain");
        return false;
    }

    auto** vtable  = *reinterpret_cast<void***>(dummySwapChain);
    gPresentTarget = vtable[kPresentVtableIndex];
    gResizeTarget  = vtable[kResizeBuffersVtableIndex];

    bool ok =
        installHook(gPresentTarget, reinterpret_cast<void*>(presentHook), reinterpret_cast<void**>(&gOriginalPresent))
        && installHook(
            gResizeTarget,
            reinterpret_cast<void*>(resizeHook),
            reinterpret_cast<void**>(&gOriginalResizeBuffers)
        );

    IDXGISwapChain1* swapChain1{};
    if (SUCCEEDED(dummySwapChain->QueryInterface(__uuidof(IDXGISwapChain1), reinterpret_cast<void**>(&swapChain1)))) {
        gPresent1Target = (*reinterpret_cast<void***>(swapChain1))[kPresent1VtableIndex];
        ok              = installHook(
                              gPresent1Target,
                              reinterpret_cast<void*>(present1Hook),
                              reinterpret_cast<void**>(&gOriginalPresent1)
                          )
                       && ok;
        swapChain1->Release();
    }
    IDXGISwapChain3* swapChain3{};
    if (SUCCEEDED(dummySwapChain->QueryInterface(__uuidof(IDXGISwapChain3), reinterpret_cast<void**>(&swapChain3)))) {
        gResize1Target = (*reinterpret_cast<void***>(swapChain3))[kResizeBuffers1VtableIndex];
        ok             = installHook(
                             gResize1Target,
                             reinterpret_cast<void*>(resize1Hook),
                             reinterpret_cast<void**>(&gOriginalResizeBuffers1)
                         )
                      && ok;
        swapChain3->Release();
    }
    dummySwapChain->Release();
    dummyContext->Release();
    dummyDevice->Release();

    // Capture the game's D3D12 direct command queue for D3D11On12 bridging.
    ID3D12Device* dummyDevice12{};
    if (SUCCEEDED(D3D12CreateDevice(
            nullptr,
            D3D_FEATURE_LEVEL_11_0,
            __uuidof(ID3D12Device),
            reinterpret_cast<void**>(&dummyDevice12)
        ))) {
        D3D12_COMMAND_QUEUE_DESC queueDesc{};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        ID3D12CommandQueue* dummyQueue{};
        if (SUCCEEDED(dummyDevice12->CreateCommandQueue(
                &queueDesc,
                __uuidof(ID3D12CommandQueue),
                reinterpret_cast<void**>(&dummyQueue)
            ))) {
            gExecuteTarget = (*reinterpret_cast<void***>(dummyQueue))[kExecuteCommandListsVtableIndex];
            ok             = installHook(
                                 gExecuteTarget,
                                 reinterpret_cast<void*>(executeCommandListsHook),
                                 reinterpret_cast<void**>(&gOriginalExecuteCommandLists)
                             )
                          && ok;
            dummyQueue->Release();
        }
        dummyDevice12->Release();
    }

    if (!ok) {
        shutdown();
        return false;
    }
    gInstalled.store(true, std::memory_order_release);
    logger().info("ImGui overlay DXGI hooks installed");
    return true;
}

bool ImGuiOverlay::installed() const { return gInstalled.load(std::memory_order_acquire); }

void ImGuiOverlay::setContent(Content content) {
    std::lock_guard lock(gContentMutex);
    gContent = std::move(content);
}

void ImGuiOverlay::setWindowDrawer(std::function<void()> drawer) {
    std::lock_guard lock(gDrawerMutex);
    gWindowDrawer = std::move(drawer);
}

void ImGuiOverlay::setInputCaptured(bool captured) {
    if (gInputCaptured.exchange(captured, std::memory_order_acq_rel) == captured) {
        return;
    }
    if (ImGui::GetCurrentContext() != nullptr) {
        auto& flags = ImGui::GetIO().ConfigFlags;
        if (captured) {
            // let ImGui pick the cursor shape (arrow, hand, text beam); the
            // window handler keeps the pointer visible through ShowCursor()
            flags &= ~ImGuiConfigFlags_NoMouseCursorChange;
        } else {
            flags |= ImGuiConfigFlags_NoMouseCursorChange;
        }
    }
    if (!captured) {
        gResetImGuiMouse.store(true, std::memory_order_release);
    }
    if (gGameWindow && ::IsWindow(gGameWindow)) {
        ::PostMessageW(gGameWindow, captured ? kMsgAcquireMenuCursor : kMsgRestoreNativeCursor, 0, 0);
    }
}

void ImGuiOverlay::drawContentPreview(
    float              x,
    float              y,
    float              width,
    float              height,
    float              scale,
    std::string const& anchor,
    float              offsetX,
    float              offsetY
) {
    Content content;
    {
        std::lock_guard cLock(gContentMutex);
        content = gContent;
    }
    if (content.lines.empty()) {
        return;
    }

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImFont*     font = ImGui::GetFont();
    float const size = ImGui::GetFontSize() * std::clamp(scale, 0.5f, 3.0f);
    float const step = size * 1.25f;
    float const padX = 6.0f;
    float const padY = 5.0f;

    // Measure the block the panel occupies...
    float maxLineWidth = 0.0f;
    auto  measure      = [&](std::vector<ImGuiOverlay::Run> const& line) {
        float width1 = 0.0f;
        for (auto const& run : line) {
            width1 += font->CalcTextSizeA(size, FLT_MAX, 0.0f, run.text.c_str()).x;
        }
        maxLineWidth = std::max(maxLineWidth, width1);
    };
    for (auto const& line : content.lines) {
        measure(line);
    }
    float const boxW = std::min(maxLineWidth + 2.0f * padX, width);
    float const boxH = std::min(static_cast<float>(content.lines.size()) * step + 2.0f * padY, height);

    // ...and place it inside the preview area exactly like the overlay places it
    // on the screen, so the anchor and both offsets are visible here as well.
    float u  = 0.5f;
    float v  = 0.5f;
    float cx = 0.0f;
    float cy = 0.0f;
    anchorOrigin(anchor, offsetX, offsetY, width, height, u, v, cx, cy);
    float boxLeft = u <= 0.001f ? cx : (u >= 0.999f ? cx - boxW : cx - boxW * 0.5f);
    float boxTop  = v <= 0.001f ? cy : (v >= 0.999f ? cy - boxH : cy - boxH * 0.5f);
    boxLeft       = std::clamp(boxLeft, 0.0f, std::max(0.0f, width - boxW));
    boxTop        = std::clamp(boxTop, 0.0f, std::max(0.0f, height - boxH));

    float const originX = x + boxLeft;
    float const originY = y + boxTop;

    if (content.background) {
        float const alpha = std::clamp(content.backgroundAlpha, 0.0f, 1.0f);
        draw->AddRectFilled(
            ImVec2(originX, originY),
            ImVec2(originX + boxW, originY + boxH),
            toImU32(0.0f, 0.0f, 0.0f, alpha)
        );
    }

    bool const left   = u < 0.34f;
    bool const right  = u > 0.66f;
    float      cursor = originY + padY;
    draw->PushClipRect(ImVec2(originX, originY), ImVec2(originX + boxW, originY + boxH), true);
    for (auto const& line : content.lines) {
        bool  empty     = true;
        float lineWidth = 0.0f;
        for (auto const& run : line) {
            if (!run.text.empty()) {
                empty = false;
            }
            lineWidth += font->CalcTextSizeA(size, FLT_MAX, 0.0f, run.text.c_str()).x;
        }
        if (empty) {
            cursor += step;
            continue;
        }
        float xCursor = originX + padX;
        if (right) {
            xCursor = originX + boxW - padX - lineWidth;
        } else if (!left) {
            xCursor = originX + (boxW - lineWidth) * 0.5f;
        }
        for (auto const& run : line) {
            auto const col  = toImU32(run.r, run.g, run.b);
            auto const colS = toImU32(0.0f, 0.0f, 0.0f, 0.75f);
            if (content.shadow) {
                draw->AddText(font, size, ImVec2(xCursor + 1.0f, cursor + 1.0f), colS, run.text.c_str());
            }
            draw->AddText(font, size, ImVec2(xCursor, cursor), col, run.text.c_str());
            xCursor += font->CalcTextSizeA(size, FLT_MAX, 0.0f, run.text.c_str()).x;
        }
        cursor += step;
    }
    draw->PopClipRect();
}

void ImGuiOverlay::shutdown() {
    if (!gInstalled.load(std::memory_order_acquire)) {
        return;
    }
    gShuttingDown.store(true, std::memory_order_release);
    removeHook(gExecuteTarget);
    removeHook(gResize1Target);
    removeHook(gPresent1Target);
    removeHook(gResizeTarget);
    removeHook(gPresentTarget);

    {
        std::lock_guard lock(gResourceMutex);
        releaseGraphicsBackend();
        if (gImGuiInitialized) {
            ImGui::DestroyContext();
            gImGuiInitialized = false;
        }
    }
    if (gGameQueue) {
        gGameQueue->Release();
        gGameQueue = nullptr;
    }
    gActiveSwapChain = nullptr;
    gPanelFont       = nullptr;
    gInstalled.store(false, std::memory_order_release);
    gShuttingDown.store(false, std::memory_order_release);
}

} // namespace insight
