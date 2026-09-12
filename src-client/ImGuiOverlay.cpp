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
std::mutex                            gContentMutex;
ImGuiOverlay::Content                 gContent;
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
        ImGui::StyleColorsDark();
        loadFonts();
        if (!ioc.Fonts->Build()) {
            logger().error("ImGui: font atlas build failed");
        }
        logger().info("ImGui font atlas: {}x{}px", ioc.Fonts->TexWidth, ioc.Fonts->TexHeight);
        gImGuiInitialized = true;
        logger().info("Injected Dear ImGui overlay initialized");
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
void drawPanel(ImGuiOverlay::Content const& content) {
    auto& io = ImGui::GetIO();
    if (io.DisplaySize.x <= 0.0f || io.DisplaySize.y <= 0.0f) {
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

    // Optional width limit (client.maxWidth, fraction of the screen): split
    // lines into visual lines at character boundaries so text never overflows.
    std::vector<std::vector<ImGuiOverlay::Run>> displayLines;
    float const maxTextWidth = content.maxWidth > 0.0f ? std::max(24.0f, content.maxWidth * w - 2.0f * padX) : 0.0f;
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

    // Anchor: fraction of the screen the panel's anchor point sits at.
    float u = 0.5f;
    float v = 0.5f;
    if (content.anchor.find("left") != std::string::npos) {
        u = 0.0f;
    } else if (content.anchor.find("right") != std::string::npos) {
        u = 1.0f;
    }
    if (content.anchor.find("top") != std::string::npos) {
        v = 0.0f;
    } else if (content.anchor.find("bottom") != std::string::npos) {
        v = 1.0f;
    }
    float cx = u * w + content.offsetX * w; // offsetX: positive = right
    float cy = v * h - content.offsetY * h; // offsetY: positive = up

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
        float alpha = std::clamp(content.backgroundAlpha, 0.0f, 1.0f);
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
            auto        col  = toImU32(run.r, run.g, run.b);
            auto        colS = toImU32(0.0f, 0.0f, 0.0f, content.background ? 0.85f : 0.5f);
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
    bool hasText = false;
    for (auto const& line : content.lines) {
        if (!line.empty()) {
            hasText = true;
            break;
        }
    }
    if (!content.visible || !hasText) {
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

    auto draw = [&](ID3D11RenderTargetView* target) {
        gDeviceContext->OMSetRenderTargets(1, &target, nullptr);
        ImGui_ImplDX11_NewFrame();
        ImGui::NewFrame();
        drawPanel(content);
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
