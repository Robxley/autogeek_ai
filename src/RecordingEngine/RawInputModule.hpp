#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <vector>
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>

namespace agk {

    enum class InputType {
        KeyDown,
        KeyUp,
        MouseDown,
        MouseUp,
        MouseMove,
        MouseWheel
    };

    struct InputEvent {
        InputType type;
        int vkCode = 0;
        double x = 0.0;
        double y = 0.0;
        int button = 0;
        double timestamp = 0.0;
        int64_t frameIndex = 0;
    };

    class TargetTracker;

    /**
     * @brief Captures system-wide Raw Input (Keyboard/Mouse) using Win32 API.
     */
    class RawInputModule {
    public:
        using EventCallback = std::function<void(const InputEvent&)>;

        RawInputModule();
        ~RawInputModule();

        bool Initialize(EventCallback callback, const TargetTracker* tracker = nullptr);
        void Start();
        void Stop();

    private:
        void MessageLoop();
        static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

        std::atomic<bool> m_isRunning;
        std::thread m_thread;
        EventCallback m_callback;
        const TargetTracker* m_tracker;
        HWND m_hwnd;
        
        // Window class registration
        static std::once_flag s_registrationFlag;
        static const wchar_t* s_className;
    };

} // namespace agk
