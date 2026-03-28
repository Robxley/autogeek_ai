#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <vector>

namespace agk {

    /**
     * @brief Helper to simulate OS-level inputs for testing the RecordingEngine's Raw Input capture.
     */
    class EventSimulator {
    public:
        static void SimulateKeyPress(WORD vkCode) {
            INPUT inputs[2] = {};
            
            inputs[0].type = INPUT_KEYBOARD;
            inputs[0].ki.wVk = vkCode;
            
            inputs[1].type = INPUT_KEYBOARD;
            inputs[1].ki.wVk = vkCode;
            inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
            
            SendInput(2, inputs, sizeof(INPUT));
        }

        static void SimulateMouseClick(int x, int y) {
            // Move cursor
            SetCursorPos(x, y);

            INPUT inputs[2] = {};
            
            inputs[0].type = INPUT_MOUSE;
            inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
            
            inputs[1].type = INPUT_MOUSE;
            inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
            
            SendInput(2, inputs, sizeof(INPUT));
        }

        // More complex simulation could go here
    };

} // namespace agk
