#pragma once

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <vector>
#include <memory>
#include <string>

namespace agk {

    using Microsoft::WRL::ComPtr;

    /**
     * @brief Result of a frame capture operation.
     */
    struct CaptureFrame {
        const uint8_t* data = nullptr;
        int linesize = 0;
        int width = 0;
        int height = 0;
        uint64_t timestamp = 0;
    };

    /**
     * @brief System responsible for high-performance screen capture using DXGI Desktop Duplication.
     */
    class DXGICapture {
    public:
        DXGICapture();
        ~DXGICapture();

        /**
         * @brief Initializes the D3D11 device and DXGI Desktop Duplication for the specified monitor.
         * @param monitorIndex Index of the monitor to capture (0 is primary).
         * @return True if initialization succeeded.
         */
        bool Initialize(int monitorIndex = 0);

        /**
         * @brief Acquires the next frame from the duplication API.
         * @param timeoutMs Timeout in milliseconds to wait for a new frame.
         * @return A CaptureFrame containing the texture and metadata, or null if timed out/failed.
         */
        std::unique_ptr<CaptureFrame> AcquireNextFrame(uint32_t timeoutMs = 100);

        /**
         * @brief Releases the current frame so the OS can continue updating the desktop.
         */
        void ReleaseFrame();

        /**
         * @brief Gets the resolution of the captured output.
         */
        void GetOutputResolution(int& width, int& height) const {
            width = m_width;
            height = m_height;
        }

    private:
        bool InitD3D11();
        bool InitDuplication(int monitorIndex);
        bool InitStagingTexture();

        ComPtr<ID3D11Device> m_d3dDevice;
        ComPtr<ID3D11DeviceContext> m_d3dContext;
        ComPtr<IDXGIOutputDuplication> m_deskDupl;
        ComPtr<ID3D11Texture2D> m_stagingTexture;

        int m_width = 0;
        int m_height = 0;
        bool m_isInitialized = false;
        bool m_hasFrameAcquired = false;
        D3D11_MAPPED_SUBRESOURCE m_mappedResource{};
    };

} // namespace agk
