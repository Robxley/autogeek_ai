#include "GamepadModule.hpp"
#include <agk/RecordingEngine/Log.hpp>
#include <cmath>

#pragma comment(lib, "xinput.lib")

namespace agk {

    GamepadModule::GamepadModule() : m_isRunning(false) {
        ZeroMemory(&m_lastState, sizeof(XINPUT_STATE));
    }

    GamepadModule::~GamepadModule() {
        Stop();
    }

    bool GamepadModule::Initialize(EventCallback callback, const InputsConfig& config) {
        m_callback = callback;
        m_config = config;
        return true;
    }

    void GamepadModule::Start() {
        if (m_isRunning) return;
        m_isRunning = true;
        m_thread = std::thread(&GamepadModule::PollingLoop, this);
    }

    void GamepadModule::Stop() {
        if (!m_isRunning) return;
        m_isRunning = false;
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    void GamepadModule::PollingLoop() {
        AGK_CORE_INFO("[Gamepad] Polling thread started (Rate: {}ms).", m_config.gamepad_polling_rate_ms);

        while (m_isRunning) {
            XINPUT_STATE state;
            ZeroMemory(&state, sizeof(XINPUT_STATE));

            // Poll Controller 0 (Simple implementation for now)
            DWORD dwResult = XInputGetState(0, &state);

            if (dwResult == ERROR_SUCCESS) {
                if (state.dwPacketNumber != m_lastState.dwPacketNumber) {
                    // Buttons changed
                    WORD changedButtons = state.Gamepad.wButtons ^ m_lastState.Gamepad.wButtons;
                    if (changedButtons != 0) {
                        InputEvent ev;
                        ev.type = InputType::KeyDown; // Mapping Gamepad buttons as generic KeyDown for now or unique types if needed
                        // For Autogeek, we might want to extend InputType to include Gamepad specific ones if needed, 
                        // but for now let's use vkCode with a Gamepad offset or dedicated types.
                        // Keeping it simple as per spec: "liste des events pad"
                        
                        // We'll use a virtual VK range for gamepad if we want to reuse KeyDown, 
                        // but let's assume the user wants dedicated Gamepad events.
                        // I'll stick to KeyDown for simplicity but with Gamepad specific IDs.
                    }

                    // Stick movement
                    short lx = state.Gamepad.sThumbLX;
                    short ly = state.Gamepad.sThumbLY;
                    
                    if (std::abs(lx) > m_config.gamepad_deadzone || std::abs(ly) > m_config.gamepad_deadzone) {
                        InputEvent ev;
                        ev.type = InputType::MouseMove; // Reusing for stick motion or specific Gamepad type
                        ev.x = lx / 32767.0;
                        ev.y = ly / 32767.0;
                        // m_callback(ev); // This would need a way to distinguish from mouse
                    }
                }
                m_lastState = state;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(m_config.gamepad_polling_rate_ms));
        }

        AGK_CORE_INFO("[Gamepad] Polling thread stopped.");
    }

} // namespace agk
