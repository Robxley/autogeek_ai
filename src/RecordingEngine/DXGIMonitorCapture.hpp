#pragma once

#include "IFrameProvider.hpp"
#include <agk/RecordingEngine/Log.hpp>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <chrono>

namespace agk {

    using Microsoft::WRL::ComPtr;

    /**
     * @brief High-performance capture module using DXGI Desktop Duplication for complete monitors.
     */
    class DXGIMonitorCapture : public IFrameProvider {
    public:
        DXGIMonitorCapture();
        ~DXGIMonitorCapture() override;

        /**
         * @brief Initializes DXGI Desktop Duplication on the specified monitor.
         * @param monitorIndex Target monitor index (0 is primary).
         */
        bool Initialize(int monitorIndex = 0);

        bool IsInitialized() const override { return m_isInitialized; }

        std::unique_ptr<CaptureFrame> AcquireNextFrame(uint32_t timeoutMs = 100) override;

        void Stop() override;

    private:
        bool InitStagingTexture();
        void ReleaseFrame();

        ComPtr<ID3D11Device> m_d3dDevice;
        ComPtr<ID3D11DeviceContext> m_d3dContext;
        ComPtr<IDXGIOutputDuplication> m_deskDupl;
        ComPtr<ID3D11Texture2D> m_stagingTexture;

        int m_width = 0;
        int m_height = 0;
        int m_monitorIndex = 0;
        bool m_isInitialized = false;
        bool m_hasFrameAcquired = false;

        D3D11_MAPPED_SUBRESOURCE m_mappedResource{};
        std::chrono::steady_clock::time_point m_startTime;
    };

} // namespace agk
