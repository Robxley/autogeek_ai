#include "DXGICapture.hpp"
#include "Log.hpp"
import RecordingEngine;

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace agk {

    DXGICapture::DXGICapture() : m_width(0), m_height(0), m_isInitialized(false), m_hasFrameAcquired(false) {}

    DXGICapture::~DXGICapture() {
        if (m_hasFrameAcquired) {
            ReleaseFrame();
        }
        m_deskDupl.Reset();
        m_d3dContext.Reset();
        m_d3dDevice.Reset();
    }

    bool DXGICapture::Initialize(int monitorIndex) {
        if (!InitD3D11()) return false;
        if (!InitDuplication(monitorIndex)) return false;
        if (!InitStagingTexture()) return false;
        
        m_isInitialized = true;
        return true;
    }

    bool DXGICapture::InitD3D11() {
        D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
        HRESULT hr = D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
            D3D11_CREATE_DEVICE_VIDEO_SUPPORT,
            featureLevels, ARRAYSIZE(featureLevels),
            D3D11_SDK_VERSION, &m_d3dDevice, nullptr, &m_d3dContext
        );

        if (FAILED(hr)) {
            AGK_CORE_ERROR("[DXGICapture] D3D11CreateDevice failed with HR: {0:x}", hr);
            return false;
        }
        return true;
    }

    bool DXGICapture::InitDuplication(int monitorIndex) {
        ComPtr<IDXGIDevice> dxgiDevice;
        if (FAILED(m_d3dDevice.As(&dxgiDevice))) return false;

        ComPtr<IDXGIAdapter> dxgiAdapter;
        if (FAILED(dxgiDevice->GetParent(IID_PPV_ARGS(&dxgiAdapter)))) return false;

        ComPtr<IDXGIOutput> dxgiOutput;
        if (FAILED(dxgiAdapter->EnumOutputs(monitorIndex, &dxgiOutput))) {
            AGK_CORE_ERROR("[DXGICapture] EnumOutputs failed for index {}", monitorIndex);
            return false;
        }

        ComPtr<IDXGIOutput1> dxgiOutput1;
        if (FAILED(dxgiOutput.As(&dxgiOutput1))) return false;

        DXGI_OUTPUT_DESC desc;
        dxgiOutput->GetDesc(&desc);
        m_width = desc.DesktopCoordinates.right - desc.DesktopCoordinates.left;
        m_height = desc.DesktopCoordinates.bottom - desc.DesktopCoordinates.top;

        HRESULT hr = dxgiOutput1->DuplicateOutput(m_d3dDevice.Get(), &m_deskDupl);
        if (FAILED(hr)) {
            AGK_CORE_ERROR("[DXGICapture] DuplicateOutput failed with HR: {0:x}", hr);
            return false;
        }

        return true;
    }

    bool DXGICapture::InitStagingTexture() {
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
            AGK_CORE_ERROR("[DXGICapture] Failed to create staging texture: {0:x}", hr);
            return false;
        }
        return true;
    }

    std::unique_ptr<CaptureFrame> DXGICapture::AcquireNextFrame(uint32_t timeoutMs) {
        if (!m_isInitialized) return nullptr;
        if (m_hasFrameAcquired) ReleaseFrame();

        DXGI_OUTDUPL_FRAME_INFO frameInfo;
        ComPtr<IDXGIResource> desktopResource;
        HRESULT hr = m_deskDupl->AcquireNextFrame(timeoutMs, &frameInfo, &desktopResource);

        if (hr == DXGI_ERROR_WAIT_TIMEOUT) return nullptr;
        if (FAILED(hr)) {
            m_isInitialized = false;
            return nullptr;
        }

        ComPtr<ID3D11Texture2D> gpuTexture;
        if (FAILED(desktopResource.As(&gpuTexture))) {
            m_deskDupl->ReleaseFrame();
            return nullptr;
        }

        // Copy from GPU to CPU-accessible Staging Texture
        m_d3dContext->CopyResource(m_stagingTexture.Get(), gpuTexture.Get());

        // Map the staging texture
        if (FAILED(m_d3dContext->Map(m_stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &m_mappedResource))) {
            m_deskDupl->ReleaseFrame();
            return nullptr;
        }

        auto frame = std::make_unique<CaptureFrame>();
        frame->data = static_cast<const uint8_t*>(m_mappedResource.pData);
        frame->linesize = m_mappedResource.RowPitch;
        frame->width = m_width;
        frame->height = m_height;
        frame->timestamp = frameInfo.LastPresentTime.QuadPart;
        
        m_hasFrameAcquired = true;
        return frame;
    }

    void DXGICapture::ReleaseFrame() {
        if (m_hasFrameAcquired) {
            m_d3dContext->Unmap(m_stagingTexture.Get(), 0);
            m_deskDupl->ReleaseFrame();
            m_hasFrameAcquired = false;
        }
    }

} // namespace agk
