#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include "TargetTracker.hpp"
#include <psapi.h>
#include <shellscalingapi.h>
#include <agk/RecordingEngine/Log.hpp>
#include <dwmapi.h>

#pragma comment(lib, "Shcore.lib")
#pragma comment(lib, "Dwmapi.lib")

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
        if (processName.empty() && windowTitle.empty()) {
            return false;
        }

        m_searchProcessName = processName;
        m_searchWindowTitle = windowTitle;
        
        TargetInfo result;
        result.hwnd = nullptr;
        
        EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(this));

        // After EnumWindows, we expect EnumWindowsProc to have populated 'this->m_pendingResult' if found
        // Let's use a simpler approach: EnumWindowsProc writes to a temporary member.
        
        if (m_pendingResult.hwnd) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_info = m_pendingResult;
            m_pendingResult.hwnd = nullptr; // Clear for next use

            // Update current state details
            if (m_clientAreaOnly) {
                GetClientRect(m_info.hwnd, &m_info.clientRect);
                m_info.width = m_info.clientRect.right - m_info.clientRect.left;
                m_info.height = m_info.clientRect.bottom - m_info.clientRect.top;

                POINT pt = {0, 0};
                ClientToScreen(m_info.hwnd, &pt);
                m_info.clientRect.left += pt.x;
                m_info.clientRect.right += pt.x;
                m_info.clientRect.top += pt.y;
                m_info.clientRect.bottom += pt.y;
            } else {
                HRESULT hr = DwmGetWindowAttribute(m_info.hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &m_info.clientRect, sizeof(m_info.clientRect));
                if (FAILED(hr)) {
                    GetWindowRect(m_info.hwnd, &m_info.clientRect);
                }
                m_info.width = m_info.clientRect.right - m_info.clientRect.left;
                m_info.height = m_info.clientRect.bottom - m_info.clientRect.top;
            }
            
            char title[MAX_PATH];
            GetWindowTextA(m_info.hwnd, title, MAX_PATH);

            if (m_info.hwnd != m_lastHwnd || m_info.processId != m_lastPid) {
                AGK_CORE_INFO("[TargetTracker] Match Found! HWND: {:p}, Title: '{}', PID: {}, Rect: {}x{} at Pos({},{})", 
                    (void*)m_info.hwnd, title, m_info.processId, m_info.width, m_info.height, m_info.clientRect.left, m_info.clientRect.top);
                m_lastHwnd = m_info.hwnd;
                m_lastPid = m_info.processId;
            }

            HWND foreground = GetForegroundWindow();
            m_info.hasFocus = (foreground == m_info.hwnd);
            m_info.isMinimized = IsIconic(m_info.hwnd);
            
            return true;
        }

        if (m_lastHwnd != nullptr) {
            AGK_CORE_WARN("[TargetTracker] Target Lost! No window matching process='{}' or title='{}'", processName, windowTitle);
            m_lastHwnd = nullptr;
            m_lastPid = 0;
        }
        return false;
    }

    bool TargetTracker::UpdateFromHWND(HWND hwnd) {
        if (!hwnd || !IsWindowVisible(hwnd)) return false;

        DWORD pid;
        GetWindowThreadProcessId(hwnd, &pid);

        std::lock_guard<std::mutex> lock(m_mutex);
        m_info.hwnd = hwnd;
        m_info.processId = pid;
        
        if (m_clientAreaOnly) {
            GetClientRect(m_info.hwnd, &m_info.clientRect);
            m_info.width = m_info.clientRect.right - m_info.clientRect.left;
            m_info.height = m_info.clientRect.bottom - m_info.clientRect.top;

            POINT pt = {0, 0};
            ClientToScreen(m_info.hwnd, &pt);
            m_info.clientRect.left += pt.x;
            m_info.clientRect.right += pt.x;
            m_info.clientRect.top += pt.y;
            m_info.clientRect.bottom += pt.y;
        } else {
            HRESULT hr = DwmGetWindowAttribute(m_info.hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &m_info.clientRect, sizeof(m_info.clientRect));
            if (FAILED(hr)) {
                GetWindowRect(m_info.hwnd, &m_info.clientRect);
            }
            m_info.width = m_info.clientRect.right - m_info.clientRect.left;
            m_info.height = m_info.clientRect.bottom - m_info.clientRect.top;
        }
        
        char title[MAX_PATH];
        GetWindowTextA(m_info.hwnd, title, MAX_PATH);
        
        static int _logThrottle = 0;
        if ((_logThrottle++ % 60) == 0) {
            AGK_CORE_INFO("[TargetTracker] HWND Updated: {:p}, Title: '{}', PID: {}, Rect: {}x{} at Pos({},{})", 
                (void*)m_info.hwnd, title, m_info.processId, m_info.width, m_info.height, m_info.clientRect.left, m_info.clientRect.top);
        }

        HWND foreground = GetForegroundWindow();
        m_info.hasFocus = (foreground == m_info.hwnd);
        m_info.isMinimized = IsIconic(m_info.hwnd);

        return true;
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
                    self->m_pendingResult.hwnd = hwnd;
                    self->m_pendingResult.processId = pid;
                    CloseHandle(process);
                    return FALSE;
                }
            }
            CloseHandle(process);
        }

        return TRUE;
    }

} // namespace agk
