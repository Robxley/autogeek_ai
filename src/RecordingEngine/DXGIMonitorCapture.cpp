#include "DXGIMonitorCapture.hpp"
#include <agk/RecordingEngine/Log.hpp>
#include <thread>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace agk {

    DXGIMonitorCapture::DXGIMonitorCapture() {
        m_startTime = std::chrono::steady_clock::now();
    }

    DXGIMonitorCapture::~DXGIMonitorCapture() {
        Stop();
    }

    bool DXGIMonitorCapture::Initialize(int monitorIndex) {
        m_monitorIndex = monitorIndex;
        
        ComPtr<IDXGIFactory1> dxgiFactory;
        HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
        if (FAILED(hr)) return false;

        ComPtr<IDXGIAdapter1> dxgiAdapter;
        ComPtr<IDXGIOutput> dxgiOutput;
        int currentMonitor = 0;
        bool found = false;

        for (UINT i = 0; dxgiFactory->EnumAdapters1(i, &dxgiAdapter) != DXGI_ERROR_NOT_FOUND; ++i) {
            for (UINT j = 0; dxgiAdapter->EnumOutputs(j, &dxgiOutput) != DXGI_ERROR_NOT_FOUND; ++j) {
                if (currentMonitor == m_monitorIndex) {
                    found = true;
                    break;
                }
                currentMonitor++;
            }
            if (found) break;
        }

        if (!found) {
            AGK_CORE_ERROR("[DXGIMonitorCapture] Monitor index {} not found.", m_monitorIndex);
            return false;
        }

        // 1. D3D11 Device (Using the SPECIFIC adapter found)
        D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
        hr = D3D11CreateDevice(dxgiAdapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
                                      0, 
                                      featureLevels, ARRAYSIZE(featureLevels), 
                                      D3D11_SDK_VERSION, &m_d3dDevice, nullptr, &m_d3dContext);
        
        if (FAILED(hr)) {
            AGK_CORE_ERROR("[DXGIMonitorCapture] Failed to create D3D11 Device on target adapter: {:08x}", (uint32_t)hr);
            return false;
        }

        ComPtr<IDXGIOutput1> dxgiOutput1;
        hr = dxgiOutput.As(&dxgiOutput1);
        if (FAILED(hr)) return false;

        // 3. DuplicateOutput
        hr = dxgiOutput1->DuplicateOutput(m_d3dDevice.Get(), &m_deskDupl);
        if (FAILED(hr)) {
            AGK_CORE_ERROR("[DXGIMonitorCapture] DuplicateOutput failed: {:08x} (Ensure application runs on correct GPU / not in specific full-screen mode)", (uint32_t)hr);
            return false;
        }

        DXGI_OUTDUPL_DESC duplDesc;
        m_deskDupl->GetDesc(&duplDesc);
        m_width = duplDesc.ModeDesc.Width;
        m_height = duplDesc.ModeDesc.Height;

        if (m_width == 0 || m_height == 0) {
            AGK_CORE_ERROR("[DXGIMonitorCapture] Invalid duplication output dimensions: {}x{}", m_width, m_height);
            return false;
        }

        // Initialize staging texture
        if (!InitStagingTexture()) {
            return false;
        }

        m_isInitialized = true;
        m_startTime = std::chrono::steady_clock::now();
        AGK_CORE_INFO("[DXGIMonitorCapture] Monitor {} Capture Initialized ({}x{})", m_monitorIndex, m_width, m_height);
        return true;
    }

    void DXGIMonitorCapture::Stop() {
        if (m_hasFrameAcquired) {
            ReleaseFrame();
        }
        m_deskDupl.Reset();
        m_stagingTexture.Reset();
        m_d3dContext.Reset();
        m_d3dDevice.Reset();
        m_isInitialized = false;
        AGK_CORE_INFO("[DXGIMonitorCapture] Stopped.");
    }

    bool DXGIMonitorCapture::InitStagingTexture() {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = m_width;
        desc.Height = m_height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.MiscFlags = 0;

        HRESULT hr = m_d3dDevice->CreateTexture2D(&desc, nullptr, &m_stagingTexture);
        if (FAILED(hr)) {
            AGK_CORE_ERROR("[DXGIMonitorCapture] Failed to create staging texture: {:08x}", (uint32_t)hr);
            return false;
        }
        return true;
    }

    std::unique_ptr<CaptureFrame> DXGIMonitorCapture::AcquireNextFrame(uint32_t timeoutMs) {
        if (!m_isInitialized || !m_deskDupl) return nullptr;

        if (m_hasFrameAcquired) {
            ReleaseFrame();
        }

        ComPtr<IDXGIResource> desktopResource;
        DXGI_OUTDUPL_FRAME_INFO frameInfo;
        
        HRESULT hr = m_deskDupl->AcquireNextFrame(timeoutMs, &frameInfo, &desktopResource);
        if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
            return nullptr; // Expected timeout when nothing changes on screen
        }

        if (hr == DXGI_ERROR_ACCESS_LOST) {
            AGK_CORE_WARN("[DXGIMonitorCapture] Access Lost! Attempting to re-initialize...");
            Stop();
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            Initialize(m_monitorIndex);
            return nullptr;
        }

        if (FAILED(hr)) {
            static int errorCount = 0;
            if (errorCount++ % 60 == 0) {
                 AGK_CORE_ERROR("[DXGIMonitorCapture] AcquireNextFrame failed: {:08x}", (uint32_t)hr);
            }
            return nullptr;
        }

        // Fast fallback: if we got a frame but the content didn't update and we don't need pointer changes
        /*if (frameInfo.AccumulatedFrames == 0 && frameInfo.LastPresentTime == 0) {
            m_deskDupl->ReleaseFrame();
            return nullptr; 
        }*/

        ComPtr<ID3D11Texture2D> desktopTexture;
        hr = desktopResource.As(&desktopTexture);
        if (FAILED(hr)) {
            m_deskDupl->ReleaseFrame();
            return nullptr;
        }

        // Copy directly from GPU to staging
        m_d3dContext->CopyResource(m_stagingTexture.Get(), desktopTexture.Get());
        
        // Unpin DesktopDuplication so system continues
        m_deskDupl->ReleaseFrame();

        // Map memory for CPU read
        if (FAILED(m_d3dContext->Map(m_stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &m_mappedResource))) {
            return nullptr;
        }

        m_hasFrameAcquired = true;

        auto result = std::make_unique<CaptureFrame>();
        result->data = static_cast<const uint8_t*>(m_mappedResource.pData);
        result->linesize = m_mappedResource.RowPitch;
        result->width = m_width;
        result->height = m_height;
        
        auto now = std::chrono::steady_clock::now();
        result->timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_startTime).count();
        
        return result;
    }

    void DXGIMonitorCapture::ReleaseFrame() {
        if (m_hasFrameAcquired) {
            m_d3dContext->Unmap(m_stagingTexture.Get(), 0);
            m_hasFrameAcquired = false;
        }
    }

} // namespace agk
