#pragma once

#include "IFrameProvider.hpp"
#include <agk/RecordingEngine/Log.hpp>
#include <d3d11.h>
#include <wrl/client.h>
#include <string>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>

// WinRT
#include <unknwn.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <winrt/Windows.System.h>

namespace agk {

    using Microsoft::WRL::ComPtr;

    /**
     * @brief Window capture module using Windows Graphics Capture (WGC).
     * Creates a dedicated server thread with a Win32 message pump to ensure FrameArrived events.
     */
    class WGCWindowCapture : public IFrameProvider {
    public:
        WGCWindowCapture();
        ~WGCWindowCapture() override;

        /**
         * @brief Initializes capture for the specified window on a dedicated message loop thread.
         * @param hwnd Window handle to capture.
         */
        bool Initialize(HWND hwnd);

        bool IsInitialized() const override { return m_isInitialized.load(); }

        std::unique_ptr<CaptureFrame> AcquireNextFrame(uint32_t timeoutMs = 100) override;

        void Stop() override;

    private:
        void ServerThreadLoop(HWND hwnd);
        bool InitWinRTSession(HWND hwnd);
        bool InitStagingTexture();
        void ReleaseFrame();

        // Standard D3D11
        ComPtr<ID3D11Device> m_d3dDevice;
        ComPtr<ID3D11DeviceContext> m_d3dContext;
        ComPtr<ID3D11Texture2D> m_stagingTexture;

        // WinRT Capture
        winrt::Windows::Graphics::Capture::GraphicsCaptureItem m_captureItem{ nullptr };
        winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool m_framePool{ nullptr };
        winrt::Windows::Graphics::Capture::GraphicsCaptureSession m_captureSession{ nullptr };
        winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::FrameArrived_revoker m_frameArrivedRevoker;
        winrt::Windows::System::DispatcherQueueController m_dispatcherController{ nullptr };

        // Server Thread Synchronization
        std::thread m_serverThread;
        std::atomic<bool> m_isInitialized{ false };
        std::atomic<bool> m_stopRequested{ false };
        DWORD m_serverThreadId = 0;

        // Shared frame state between Server Thread and Reading Thread
        std::mutex m_frameMutex;
        winrt::Windows::Graphics::Capture::Direct3D11CaptureFrame m_currentFrame{ nullptr };
        std::atomic<bool> m_newFrameAvailable{ false };

        int m_width = 0;
        int m_height = 0;
        bool m_hasFrameAcquired = false;
        D3D11_MAPPED_SUBRESOURCE m_mappedResource{};
        std::chrono::steady_clock::time_point m_startTime;
    };

} // namespace agk
