#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>
#include <mutex>

namespace agk {

    /**
     * @brief Structure representing the current state of the target being recorded.
     */
    struct TargetInfo {
        HWND hwnd = nullptr;
        RECT clientRect = {0, 0, 0, 0};
        DWORD processId = 0;
        bool hasFocus = false;
        bool isMinimized = false;
        int width = 0;
        int height = 0;
    };

    /**
     * @brief System responsible for tracking the target window and providing its geometry for input normalization.
     */
    class TargetTracker {
    public:
        TargetTracker();
        ~TargetTracker() = default;

        /**
         * @brief Updates the search for the target window based on process name or title.
         */
        bool Update(const std::string& processName, const std::string& windowTitle);

        /**
         * @brief Updates tracking explicitly via HWND.
         */
        bool UpdateFromHWND(HWND hwnd);

        /**
         * @brief Normalizes a screen coordinate to a 0.0-1.0 range relative to the target's client area.
         */
        void NormalizeCoordinates(int screenX, int screenY, double& outX, double& outY) const;

        const TargetInfo& GetInfo() const { return m_info; }
        
        /**
         * @brief Enables Per-Monitor DPI awareness for the calling process.
         */
        static void EnableDpiAwareness();

    private:
        static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);
        
        TargetInfo m_info;
        TargetInfo m_pendingResult; // Temporary for EnumWindowsProc
        std::string m_searchProcessName;
        std::string m_searchWindowTitle;
        mutable std::mutex m_mutex;
    };

} // namespace agk
