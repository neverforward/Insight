#include "ImeField.h"

#include <Windows.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>

#include "Insight.h"

namespace insight::ImeField {
namespace {

std::recursive_mutex  gLock;
std::atomic<bool>     gActive{false};
std::atomic<HWND>     gPopup{nullptr};
std::atomic<HWND>     gEdit{nullptr};
std::atomic<int>      gTargetX{0};
std::atomic<int>      gTargetY{0};
std::atomic<int>      gTargetW{0};
std::atomic<int>      gTargetH{0};
std::atomic<bool>     gReposition{false};
std::atomic<uint64_t> gFrame{0};
std::atomic<uint64_t> gLastShown{0};
std::atomic<int>      gRefocus{0};
std::atomic<int64_t>  gLastEnd{0};

HWND           gOwner   = nullptr;
WNDPROC        gEditOld = nullptr;
char*          gBuffer  = nullptr;
size_t         gSize    = 0;
char           gText[4096] = {};
std::once_flag gClassOnce;
constexpr UINT kMsgRefocus = WM_USER + 102;
constexpr UINT kMsgMove    = WM_USER + 103;

void trace(char const* text) { Insight::getInstance().getSelf().getLogger().info("[ime] {}", text); }

void trace(char const* text, int value) {
    Insight::getInstance().getSelf().getLogger().info("[ime] {} {}", text, value);
}

int64_t nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

std::wstring toWide(std::string const& utf8) {
    if (utf8.empty()) {
        return {};
    }
    int const length = ::MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
    if (length <= 0) {
        return {};
    }
    std::wstring wide(static_cast<size_t>(length), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), wide.data(), length);
    return wide;
}

std::string toUtf8(std::wstring const& wide) {
    if (wide.empty()) {
        return {};
    }
    int const length = ::WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (length <= 0) {
        return {};
    }
    std::string utf8(static_cast<size_t>(length), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), utf8.data(), length, nullptr, nullptr);
    return utf8;
}

bool composing(HWND window) {
    HIMC const context = ::ImmGetContext(window);
    if (context == nullptr) {
        return false;
    }
    LONG const length = ::ImmGetCompositionStringW(context, GCS_COMPSTR, nullptr, 0);
    ::ImmReleaseContext(window, context);
    return length > 0;
}

HBRUSH backgroundBrush() {
    static HBRUSH brush = ::CreateSolidBrush(RGB(0x18, 0x18, 0x1b));
    return brush;
}

HBRUSH borderBrush() {
    static HBRUSH brush = ::CreateSolidBrush(RGB(0xd4, 0xd4, 0xd8));
    return brush;
}

// Copies what the edit control holds into the caller's buffer (UTF-8, truncated to fit).
void syncFromEdit(HWND edit) {
    if (edit == nullptr || gBuffer == nullptr || gSize == 0) {
        return;
    }
    int const length = ::GetWindowTextLengthW(edit);
    if (length < 0) {
        return;
    }
    std::wstring wide(static_cast<size_t>(length) + 1, L'\0');
    ::GetWindowTextW(edit, wide.data(), length + 1);
    wide.resize(static_cast<size_t>(length));
    std::string utf8 = toUtf8(wide);
    while (!wide.empty() && utf8.size() >= gSize) {
        wide.pop_back();
        utf8 = toUtf8(wide);
        ::SetWindowTextW(edit, wide.c_str());
        ::SendMessageW(edit, EM_SETSEL, static_cast<WPARAM>(wide.size()), static_cast<LPARAM>(wide.size()));
    }
    std::lock_guard lock(gLock);
    std::snprintf(gText, sizeof(gText), "%s", utf8.c_str());
    std::snprintf(gBuffer, gSize, "%s", utf8.c_str());
}

// The game re-activates its own window and takes the focus back the moment the field appears, so
// losing it is normal: take it back while attempts are left, and only end the edit when it is really
// gone (the player clicked elsewhere, or switched to another application).
bool takeFocusBack() {
    int const taken = gRefocus.load();
    if (taken >= 5) {
        return false;
    }
    gRefocus.store(taken + 1);
    if (HWND const popup = gPopup.load(); popup != nullptr) {
        ::PostMessageW(popup, kMsgRefocus, 0, 0);
    }
    return true;
}

LRESULT CALLBACK editProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_KEYDOWN:
        if (wParam >= VK_F1 && wParam <= VK_F24) {
            if (gOwner != nullptr) {
                ::PostMessageW(gOwner, message, wParam, lParam);
            }
            return 0;
        }
        if (wParam == VK_RETURN && !composing(window)) {
            end();
            return 0;
        }
        if (wParam == VK_ESCAPE || wParam == VK_TAB) {
            end();
            return 0;
        }
        if (wParam == 'A' && (::GetKeyState(VK_CONTROL) & 0x8000) != 0) {
            ::SendMessageW(window, EM_SETSEL, 0, -1);
            return 0;
        }
        break;
    case WM_KILLFOCUS: {
        HWND const next = reinterpret_cast<HWND>(wParam);
        if (next == window || next == gEdit.load() || composing(window)) {
            break;
        }
        if (next == gOwner) {
            if (takeFocusBack()) {
                return 0;
            }
        }
        end();
        return 0;
    }
    case WM_SETCURSOR:
        ::SetCursor(::LoadCursorW(nullptr, IDC_IBEAM));
        return TRUE;
    default:
        break;
    }
    return ::CallWindowProcW(gEditOld, window, message, wParam, lParam);
}

LRESULT CALLBACK popupProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        RECT client{};
        ::GetClientRect(window, &client);
        int const width  = client.right - client.left;
        int const height = client.bottom - client.top;
        int const fontH  = -static_cast<int>(std::max(13.0f, std::round(static_cast<float>(height - 4) * 0.62f)));
        HFONT const font = ::CreateFontW(
            fontH, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei"
        );
        ::SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(font));
        HWND const edit = ::CreateWindowExW(
            0, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_LEFT,
            4, 2, std::max(10, width - 8), std::max(12, height - 4),
            window, reinterpret_cast<HMENU>(1), nullptr, nullptr
        );
        gEdit.store(edit);
        if (edit == nullptr) {
            trace("EDIT control creation failed");
            return 0;
        }
        if (gSize > 1) {
            ::SendMessageW(edit, EM_SETLIMITTEXT, static_cast<WPARAM>(gSize - 1), 0);
        }
        ::SendMessageW(edit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(1, 1));
        ::SendMessageW(edit, WM_SETFONT, reinterpret_cast<WPARAM>(font), MAKELPARAM(TRUE, 0));
        std::wstring initial;
        {
            std::lock_guard lock(gLock);
            initial = toWide(gText);
        }
        ::SetWindowTextW(edit, initial.c_str());
        ::SendMessageW(edit, EM_SETSEL, static_cast<WPARAM>(initial.size()), static_cast<LPARAM>(initial.size()));
        gEditOld = reinterpret_cast<WNDPROC>(
            ::SetWindowLongPtrW(edit, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(editProc))
        );
        trace("popup ready");
        return 0;
    }
    case WM_COMMAND:
        if (HIWORD(wParam) == EN_CHANGE) {
            syncFromEdit(reinterpret_cast<HWND>(lParam));
        } else if (HIWORD(wParam) == EN_KILLFOCUS) {
            if (!composing(gEdit.load()) && !takeFocusBack()) {
                end();
            }
        }
        return 0;
    case kMsgRefocus:
        ::SetForegroundWindow(window);
        ::SetActiveWindow(window);
        ::SetFocus(gEdit.load());
        trace("refocused");
        return 0;
    case kMsgMove:
        if (gReposition.exchange(false)) {
            int const x = gTargetX.load();
            int const y = gTargetY.load();
            int const w = gTargetW.load();
            int const h = gTargetH.load();
            ::SetWindowPos(window, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
            if (HWND const edit = gEdit.load(); edit != nullptr) {
                ::SetWindowPos(edit, nullptr, 4, 2, std::max(10, w - 8), std::max(12, h - 4), SWP_NOZORDER | SWP_NOACTIVATE);
            }
        }
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC: {
        HDC const dc = reinterpret_cast<HDC>(wParam);
        ::SetTextColor(dc, RGB(0xfa, 0xfa, 0xfa));
        ::SetBkColor(dc, RGB(0x18, 0x18, 0x1b));
        return reinterpret_cast<LRESULT>(backgroundBrush());
    }
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC const   dc = ::BeginPaint(window, &ps);
        RECT        client{};
        ::GetClientRect(window, &client);
        ::FillRect(dc, &client, backgroundBrush());
        RECT const top{0, 0, client.right, 2};
        RECT const bottom{0, client.bottom - 2, client.right, client.bottom};
        RECT const left{0, 0, 2, client.bottom};
        RECT const right{client.right - 2, 0, client.right, client.bottom};
        ::FillRect(dc, &top, borderBrush());
        ::FillRect(dc, &bottom, borderBrush());
        ::FillRect(dc, &left, borderBrush());
        ::FillRect(dc, &right, borderBrush());
        ::EndPaint(window, &ps);
        return 0;
    }
    case WM_KILLFOCUS:
    case WM_ACTIVATE:
        if (message == WM_KILLFOCUS || LOWORD(wParam) == WA_INACTIVE) {
            HWND const focus = ::GetFocus();
            if (focus != window && focus != gEdit.load() && !takeFocusBack()) {
                end();
            }
        }
        return ::DefWindowProcW(window, message, wParam, lParam);
    case WM_SETCURSOR:
        ::SetCursor(::LoadCursorW(nullptr, IDC_IBEAM));
        return TRUE;
    case WM_CLOSE:
        if (HFONT const font = reinterpret_cast<HFONT>(::GetWindowLongPtrW(window, GWLP_USERDATA)); font != nullptr) {
            ::DeleteObject(font);
            ::SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        }
        ::DestroyWindow(window);
        ::PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return ::DefWindowProcW(window, message, wParam, lParam);
}

void run(HWND owner) {
    HINSTANCE const instance = ::GetModuleHandleW(nullptr);
    std::call_once(gClassOnce, [instance]() {
        WNDCLASSW wc{};
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = popupProc;
        wc.hInstance     = instance;
        wc.lpszClassName = L"InsightImeField";
        wc.hCursor       = ::LoadCursorW(nullptr, IDC_IBEAM);
        wc.hbrBackground = nullptr;
        trace("class registered");
        ::RegisterClassW(&wc);
    });

    HWND const window = ::CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        L"InsightImeField",
        L"",
        WS_POPUP | WS_CLIPCHILDREN,
        gTargetX.load(),
        gTargetY.load(),
        gTargetW.load(),
        gTargetH.load(),
        owner,
        nullptr,
        instance,
        nullptr
    );
    if (window == nullptr) {
        trace("popup creation failed");
        gActive.store(false, std::memory_order_release);
        return;
    }
    gPopup.store(window);

    // The game's window belongs to another thread; without sharing the input state its own
    // set-foreground wins immediately and the edit closes itself.
    DWORD const ourThread   = ::GetCurrentThreadId();
    DWORD const ownerThread = owner != nullptr ? ::GetWindowThreadProcessId(owner, nullptr) : 0;
    bool const  attached    = ownerThread != 0 && ownerThread != ourThread
                          && ::AttachThreadInput(ourThread, ownerThread, TRUE) != FALSE;
    trace("popup up (thread attached {})", attached);

    ::ShowWindow(window, SW_SHOW);
    ::UpdateWindow(window);
    ::SetForegroundWindow(window);
    ::SetActiveWindow(window);
    ::SetFocus(::GetDlgItem(window, 1));
    if (::GetFocus() != gEdit.load()) {
        trace("focus did not take");
    }

    MSG message{};
    while (::GetMessageW(&message, nullptr, 0, 0)) {
        ::TranslateMessage(&message);
        ::DispatchMessageW(&message);
    }
    if (attached) {
        ::AttachThreadInput(ourThread, ownerThread, FALSE);
    }
    gPopup.store(nullptr);
    gEdit.store(nullptr);
    trace("popup gone");
}

} // namespace

void setOwnerWindow(void* window) { gOwner = static_cast<HWND>(window); }

bool begin(char const* what, char* buffer, size_t size, ImVec2 const& itemMin, ImVec2 const& itemMax) {
    (void)what; // the label is only useful in a log line, and the log is off by default
    if (buffer == nullptr || size == 0) {
        return false;
    }
    if (gOwner == nullptr) {
        gOwner = ::FindWindowW(L"Bedrock", L"Minecraft");
        if (gOwner == nullptr) {
            gOwner = ::FindWindowW(L"Bedrock", nullptr);
        }
    }
    if (gOwner == nullptr) {
        trace("no game window, cannot place the field");
        return false;
    }
    // A click that ends one edit must not open the next one in the same breath.
    if (nowMs() - gLastEnd.load() < 300) {
        return false;
    }
    POINT topLeft{static_cast<LONG>(itemMin.x), static_cast<LONG>(itemMin.y)};
    POINT bottomRight{static_cast<LONG>(itemMax.x), static_cast<LONG>(itemMax.y)};
    ::ClientToScreen(gOwner, &topLeft);
    ::ClientToScreen(gOwner, &bottomRight);
    int const width  = bottomRight.x - topLeft.x;
    int const height = bottomRight.y - topLeft.y;
    if (width < 10 || height < 10) {
        trace("field too small to cover");
        return false;
    }
    {
        std::lock_guard lock(gLock);
        if (gActive.load()) {
            if (gBuffer == buffer) {
                return true;
            }
            end();
        }
        gBuffer = buffer;
        gSize   = size;
        std::snprintf(gText, sizeof(gText), "%s", buffer);
        gTargetX.store(topLeft.x);
        gTargetY.store(topLeft.y);
        gTargetW.store(width);
        gTargetH.store(height);
        gRefocus.store(0);
        gLastShown.store(gFrame.load());
        gActive.store(true, std::memory_order_release);
    }
    // ImGui must forget this widget, or its cached text is written back over what the native control
    // puts in the buffer when the widget loses the id.
    if (ImGui::GetCurrentContext() != nullptr) {
        ImGui::ClearActiveID();
    }
    trace("begin");
    HWND const owner = gOwner;
    std::thread([owner]() { run(owner); }).detach();
    return true;
}

void end() {
    std::lock_guard lock(gLock);
    if (!gActive.load()) {
        return;
    }
    if (HWND const edit = gEdit.load(); edit != nullptr && ::IsWindow(edit)) {
        syncFromEdit(edit);
    }
    gActive.store(false, std::memory_order_release);
    gLastEnd.store(nowMs());
    gBuffer = nullptr;
    gSize   = 0;
    if (HWND const popup = gPopup.load(); popup != nullptr && ::IsWindow(popup)) {
        ::PostMessageW(popup, WM_CLOSE, 0, 0);
    }
    if (ImGui::GetCurrentContext() != nullptr) {
        ImGui::ClearActiveID();
    }
}

bool active() { return gActive.load(std::memory_order_acquire); }

bool belongsTo(char const* buffer) {
    std::lock_guard lock(gLock);
    return gActive.load() && gBuffer == buffer;
}

void follow(ImVec2 const& itemMin, ImVec2 const& itemMax) {
    HWND const popup = gPopup.load();
    if (!active() || gOwner == nullptr || popup == nullptr || !::IsWindow(popup)) {
        return;
    }
    POINT topLeft{static_cast<LONG>(itemMin.x), static_cast<LONG>(itemMin.y)};
    POINT bottomRight{static_cast<LONG>(itemMax.x), static_cast<LONG>(itemMax.y)};
    ::ClientToScreen(gOwner, &topLeft);
    ::ClientToScreen(gOwner, &bottomRight);
    int const width  = bottomRight.x - topLeft.x;
    int const height = bottomRight.y - topLeft.y;
    if (width <= 0 || height <= 0) {
        return;
    }
    gLastShown.store(gFrame.load()); // the screen is still drawing this field
    if (gTargetX.load() == topLeft.x && gTargetY.load() == topLeft.y && gTargetW.load() == width
        && gTargetH.load() == height) {
        return;
    }
    gTargetX.store(topLeft.x);
    gTargetY.store(topLeft.y);
    gTargetW.store(width);
    gTargetH.store(height);
    gReposition.store(true);
    ::PostMessageW(popup, kMsgMove, 0, 0);
}

char const* value() { return gText; }

void tick() {
    uint64_t const now = gFrame.fetch_add(1) + 1;
    if (gActive.load() && now > gLastShown.load() + 10) {
        trace("screen stopped showing the field, closing");
        end();
    }
}

} // namespace insight::ImeField
