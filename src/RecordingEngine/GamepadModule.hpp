#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <Xinput.h>
#include <functional>
#include <thread>
#include <atomic>
#include "RawInputModule.hpp"
#include <agk/RecordingEngine/ConfigSystem.hpp>

namespace agk {

    /**
     * @brief Polls Gamepad state using XInput API.
     */
    class GamepadModule {
    public:
        using EventCallback = std::function<void(const InputEvent&)>;

        GamepadModule();
        ~GamepadModule();

        bool Initialize(EventCallback callback, const InputsConfig& config);
        void Start();
        void Stop();

    private:
        void PollingLoop();

        InputsConfig m_config;
        EventCallback m_callback;
        
        std::atomic<bool> m_isRunning;
        std::thread m_thread;

        // Last state for change detection
        XINPUT_STATE m_lastState;
    };

} // namespace agk
