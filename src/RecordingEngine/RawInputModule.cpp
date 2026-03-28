#include "RawInputModule.hpp"
#include "TargetTracker.hpp"
#include <agk/RecordingEngine/Log.hpp>
#include <iostream>

#include <mutex>
#include <vector>

namespace agk {

    std::once_flag RawInputModule::s_registrationFlag;
    const wchar_t* RawInputModule::s_className = L"RawInputModuleWindow";

    RawInputModule::RawInputModule() : m_isRunning(false), m_hwnd(nullptr), m_tracker(nullptr) {}

    RawInputModule::~RawInputModule() {
        Stop();
    }

    bool RawInputModule::Initialize(EventCallback callback, const TargetTracker* tracker) {
        m_callback = callback;
        m_tracker = tracker;
        return true;
    }

    void RawInputModule::Start() {
        if (m_isRunning) return;
        m_isRunning = true;
        m_thread = std::thread(&RawInputModule::MessageLoop, this);
    }

    void RawInputModule::Stop() {
        if (!m_isRunning) return;
        m_isRunning = false;
        
        if (m_hwnd) {
            PostMessage(m_hwnd, WM_CLOSE, 0, 0);
        }

        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    void RawInputModule::MessageLoop() {
        // Step 1: Register window class (one-time, thread-safe)
        std::call_once(s_registrationFlag, []() {
            WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
            wc.lpfnWndProc = WindowProc;
            wc.hInstance = GetModuleHandle(nullptr);
            wc.lpszClassName = s_className;
            if (!RegisterClassExW(&wc)) {
                AGK_CORE_ERROR("[RawInput] Failed to register window class (Error: {:08x})", GetLastError());
            }
        });

        // Step 2: Create a message-only window
        m_hwnd = CreateWindowExW(0, s_className, L"RawInputWindow", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, GetModuleHandle(nullptr), this);
        if (!m_hwnd) {
            AGK_CORE_ERROR("[RawInput] Failed to create message-only window");
            return;
        }

        // Step 3: Register Raw Input devices
        RAWINPUTDEVICE rid[2];
        
        // Keyboard
        rid[0].usUsagePage = 0x01;
        rid[0].usUsage = 0x06;
        rid[0].dwFlags = RIDEV_INPUTSINK; // Capture even without focus
        rid[0].hwndTarget = m_hwnd;

        // Mouse
        rid[1].usUsagePage = 0x01;
        rid[1].usUsage = 0x02;
        rid[1].dwFlags = RIDEV_INPUTSINK; 
        rid[1].hwndTarget = m_hwnd;

        if (!RegisterRawInputDevices(rid, 2, sizeof(RAWINPUTDEVICE))) {
            AGK_CORE_ERROR("[RawInput] Failed to register raw input devices");
            return;
        }

        AGK_CORE_INFO("[RawInput] Devices registered and message loop started.");

        // Step 4: Run the message loop
        MSG msg;
        while (GetMessage(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (!m_isRunning) break;
        }

        AGK_CORE_INFO("[RawInput] Message loop stopped.");
    }

    LRESULT CALLBACK RawInputModule::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        if (uMsg == WM_INPUT) {
            RawInputModule* self = reinterpret_cast<RawInputModule*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
            if (!self) {
                // Initial set of USERDATA at creation
                if (CREATESTRUCT* cs = (CREATESTRUCT*)lParam) {
                    self = (RawInputModule*)cs->lpCreateParams;
                    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)self);
                }
            }

            if (self && self->m_callback) {
                UINT dwSize;
                GetRawInputData((HRAWINPUT)lParam, RID_INPUT, nullptr, &dwSize, sizeof(RAWINPUTHEADER));
                std::vector<BYTE> lpb(dwSize);
                if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb.data(), &dwSize, sizeof(RAWINPUTHEADER)) == dwSize) {
                    RAWINPUT* raw = (RAWINPUT*)lpb.data();
                    InputEvent event;
                    
                    if (raw->header.dwType == RIM_TYPEKEYBOARD) {
                        event.type = (raw->data.keyboard.Flags & RI_KEY_BREAK) ? InputType::KeyUp : InputType::KeyDown;
                        event.vkCode = raw->data.keyboard.VKey;
                        self->m_callback(event);
                    }
                    else if (raw->header.dwType == RIM_TYPEMOUSE) {
                        POINT pt;
                        GetCursorPos(&pt);
                        
                        if (self->m_tracker) {
                            self->m_tracker->NormalizeCoordinates(pt.x, pt.y, event.x, event.y);
                        } else {
                            event.x = (double)pt.x / GetSystemMetrics(SM_CXSCREEN);
                            event.y = (double)pt.y / GetSystemMetrics(SM_CYSCREEN);
                        }

                        USHORT flags = raw->data.mouse.usButtonFlags;
                        // Always send move on any raw mouse change (for precision)
                        if (raw->data.mouse.lLastX != 0 || raw->data.mouse.lLastY != 0) {
                            event.type = InputType::MouseMove;
                            self->m_callback(event);
                        }

                        if (flags & RI_MOUSE_LEFT_BUTTON_DOWN) {
                            event.type = InputType::MouseDown;
                            event.button = 0;
                            self->m_callback(event);
                        } else if (flags & RI_MOUSE_LEFT_BUTTON_UP) {
                            event.type = InputType::MouseUp;
                            event.button = 0;
                            self->m_callback(event);
                        }
                    }
                }
            }
            return 0;
        }
        
        if (uMsg == WM_NCCREATE) {
            CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }

        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

} // namespace agk
