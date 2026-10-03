#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

namespace insight {

// Client-side configuration screen, drawn with Dear ImGui inside the overlay's
// frame. Opened with the configured hotkey or `/insight gui`.
//
// Input handling: LeviLamina delivers key/mouse input on the game thread while
// ImGui renders on the render thread, so events are queued here and replayed
// into ImGui::GetIO() right before the window is drawn. While the screen is
// visible the caller also cancels the game's own input events, which makes the
// screen modal (the player neither moves nor looks around).
//
// Config writes: the screen runs on the render thread, so it never touches the
// config itself - it queues an edit which the game thread applies through
// Insight::applyConfigEdit() (that keeps disk writes and gConfig off the render
// thread) and reports the result back here.
class ConfigUi {
public:
    static ConfigUi& instance();

    [[nodiscard]] bool visible() const { return mVisible.load(std::memory_order_acquire); }
    void               setVisible(bool visible);
    void               toggle();

    // --- game thread ------------------------------------------------------
    /// Replays a queued edit (option/value) through applyConfigEdit(); "reload"
    /// re-reads the config file. Returns false when there is nothing pending.
    bool takeEdit(std::string& option, std::string& value);

    /// Result message of the last edit, shown at the bottom of the screen.
    void setStatus(bool ok, std::string message);

    /// Locale used for the screen's own labels.
    void setLocale(std::string locale);

    /// Text shown in the preview box (the panel the client currently renders).
    void setPreviewText(std::string text);

    // --- input -----------------------------------------------------------
    /// Queued on the game thread; used only for "press a key to bind", every
    /// other event comes from ImGui's Win32 backend.
    void onKey(int vkCode, bool down);

    /// True while the keyboard belongs to this screen rather than to a hotkey: a
    /// rebind capture is waiting for a key, or one of its text fields has the caret.
    /// Read from the game thread before a hotkey is allowed to fire, so typing into
    /// the screen's own fields cannot close it by accident.
    [[nodiscard]] bool wantsKeyboard() const {
        return mWantTextInput.load(std::memory_order_relaxed) || mCapturingKey.load(std::memory_order_relaxed) >= 0;
    }

    // --- rendering (render thread, inside the ImGui frame) ---------------
    void draw();

    /// Queued by the screen, consumed on the game thread by takeEdit().
    void queueEdit(std::string option, std::string value);

private:
    ConfigUi() = default;

    enum class EventKind { KeyDown, KeyUp };
    struct Event {
        EventKind kind = EventKind::KeyDown;
        int       code = 0;
    };

    std::atomic<bool> mVisible{false};

    std::mutex         mMutex;
    std::vector<Event> mEvents;
    std::string        mPreviewText;
    std::string        mLocale;
    std::string        mStatusMessage;
    bool               mStatusOk = true;

    // pending edits (screen -> game thread), applied in order
    struct Edit {
        std::string option;
        std::string value;
    };
    std::vector<Edit> mPending;

    // screen state (render thread only)
    std::string mOpenRow;
    // 0 = open screen, 1 = toggle info. Atomic because wantsKeyboard() is read from the
    // game thread while the screen sets it on the render thread.
    std::atomic<int> mCapturingKey{-1};
    // Whether an ImGui text field of this screen has the caret (published by draw()).
    std::atomic<bool> mWantTextInput{false};
    // text rows keep one persistent buffer: refilling it from the config every
    // frame would fight with ImGui's own edit state (and make the field
    // unusable after the first edit)
    char        mEditBuffer[1024] = {};
    std::string mEditRow;
    // slider rows keep one value too: re-reading the config on every frame would
    // undo a drag (ImGui adds the drag delta to the value it is handed)
    std::string mSliderRow;
    float       mSliderValue = 0.0f;
};

} // namespace insight
