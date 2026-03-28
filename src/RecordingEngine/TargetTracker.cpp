#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include "TargetTracker.hpp"
#include <psapi.h>
#include <shellscalingapi.h>
#include "Log.hpp"

#pragma comment(lib, "Shcore.lib")

// Support for older Windows SDKs
#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((HANDLE)-4)
#endif

namespace agk {

    TargetTracker::TargetTracker() {}

    void TargetTracker::EnableDpiAwareness() {
        // Preference for Per-Monitor V2 (Windows 10 1703+)
        // Use SetProcessDpiAwarenessContext (requires User32.lib)
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        
        AGK_CORE_INFO("[Engine] DPI Awareness enabled (Per-Monitor).");
    }

    bool TargetTracker::Update(const std::string& processName, const std::string& windowTitle) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_searchProcessName = processName;
        m_searchWindowTitle = windowTitle;
        
        m_info.hwnd = nullptr; // Reset before search
        EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(this));

        if (m_info.hwnd) {
            // Update current state
            GetClientRect(m_info.hwnd, &m_info.clientRect);
            m_info.width = m_info.clientRect.right - m_info.clientRect.left;
            m_info.height = m_info.clientRect.bottom - m_info.clientRect.top;
            
            HWND foreground = GetForegroundWindow();
            m_info.hasFocus = (foreground == m_info.hwnd);
            m_info.isMinimized = IsIconic(m_info.hwnd);
            
            return true;
        }

        return false;
    }

    void TargetTracker::NormalizeCoordinates(int screenX, int screenY, double& outX, double& outY) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (!m_info.hwnd) {
            // Fallback to primary monitor normalization if no target found
            outX = (double)screenX / GetSystemMetrics(SM_CXSCREEN);
            outY = (double)screenY / GetSystemMetrics(SM_CYSCREEN);
            return;
        }

        POINT pt = { screenX, screenY };
        ScreenToClient(m_info.hwnd, &pt);

        if (m_info.width > 0 && m_info.height > 0) {
            outX = (double)pt.x / m_info.width;
            outY = (double)pt.y / m_info.height;
        } else {
            outX = 0;
            outY = 0;
        }
    }

    BOOL CALLBACK TargetTracker::EnumWindowsProc(HWND hwnd, LPARAM lParam) {
        TargetTracker* self = reinterpret_cast<TargetTracker*>(lParam);
        
        if (!IsWindowVisible(hwnd)) return TRUE;

        DWORD pid;
        GetWindowThreadProcessId(hwnd, &pid);

        HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (process) {
            char filename[MAX_PATH];
            if (GetModuleBaseNameA(process, NULL, filename, MAX_PATH)) {
                std::string sProcessName(filename);

                bool match = false;
                if (!self->m_searchProcessName.empty() && sProcessName.find(self->m_searchProcessName) != std::string::npos) {
                    match = true;
                }

                if (!match && !self->m_searchWindowTitle.empty()) {
                    char title[MAX_PATH];
                    GetWindowTextA(hwnd, title, MAX_PATH);
                    std::string sTitle(title);
                    
                    if (sTitle.find(self->m_searchWindowTitle) != std::string::npos) {
                        match = true;
                    }
                }

                if (match) {
                    self->m_info.hwnd = hwnd;
                    self->m_info.processId = pid;
                    CloseHandle(process);
                    return FALSE;
                }
            }
            CloseHandle(process);
        }

        return TRUE;
    }

} // namespace agk
