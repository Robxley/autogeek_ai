#include "WGCWindowCapture.hpp"
#include <agk/RecordingEngine/Log.hpp>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directX.direct3d11.interop.h>
#include <winrt/Windows.Foundation.Metadata.h>
#include <dispatcherqueue.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace agk {

    template <typename T>
    auto GetDXGIInterface(winrt::Windows::Foundation::IInspectable const& inspectable)
    {
        winrt::com_ptr<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess> access;
        winrt::check_hresult(inspectable.as(winrt::guid_of<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>(), access.put_void()));
        winrt::com_ptr<T> result;
        winrt::check_hresult(access->GetInterface(winrt::guid_of<T>(), result.put_void()));
        return result;
    }

    WGCWindowCapture::WGCWindowCapture() {
        m_startTime = std::chrono::steady_clock::now();
    }

    WGCWindowCapture::~WGCWindowCapture() {
        Stop();
    }

    bool WGCWindowCapture::Initialize(HWND hwnd) {
        if (m_isInitialized.load()) return true;

        m_stopRequested.store(false);
        m_serverThread = std::thread(&WGCWindowCapture::ServerThreadLoop, this, hwnd);

        // Wait for thread to initialize up to 2 seconds
        auto start = std::chrono::steady_clock::now();
        while (!m_isInitialized.load() && !m_stopRequested.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() > 2000) {
                AGK_CORE_ERROR("[WGCWindowCapture] Timeout waiting for server thread to initialize.");
                Stop();
                return false;
            }
        }
        return m_isInitialized.load();
    }

    void WGCWindowCapture::Stop() {
        if (!m_stopRequested.load()) {
            m_stopRequested.store(true);
            if (m_serverThreadId != 0) {
                PostThreadMessage(m_serverThreadId, WM_QUIT, 0, 0);
            }
            if (m_serverThread.joinable()) {
                m_serverThread.join();
            }
            if (m_hasFrameAcquired) {
                ReleaseFrame();
            }
            m_d3dContext.Reset();
            m_d3dDevice.Reset();
            AGK_CORE_INFO("[WGCWindowCapture] Stopped.");
        }
    }

    void WGCWindowCapture::ServerThreadLoop(HWND hwnd) {
        m_serverThreadId = GetCurrentThreadId();

        AGK_CORE_INFO("[WGCWindowCapture] Server Thread started (TID: {})", m_serverThreadId);

        // WinRT Initialization
        try {
            winrt::init_apartment(winrt::apartment_type::single_threaded);
        } catch (...) {
            // Might already be initialized
        }

        // Create a DispatcherQueue on this CURRENT thread.
        DispatcherQueueOptions options = {};
        options.dwSize = sizeof(DispatcherQueueOptions);
        options.threadType = DQTYPE_THREAD_CURRENT;
        options.apartmentType = static_cast<DISPATCHERQUEUE_THREAD_APARTMENTTYPE>(2); // DQTAT_COM_STA

        ABI::Windows::System::IDispatcherQueueController* controller = nullptr;
        HRESULT hr = CreateDispatcherQueueController(options, &controller);
        if (FAILED(hr)) {
            AGK_CORE_ERROR("[WGCWindowCapture] Failed to create DispatcherQueueController: {:08x}", (uint32_t)hr);
            m_stopRequested.store(true);
            return;
        }
        winrt::copy_from_abi(m_dispatcherController, controller);

        // Initialize actual capture
        if (!InitWinRTSession(hwnd)) {
            m_dispatcherController = nullptr;
            m_stopRequested.store(true);
            return;
        }

        m_isInitialized.store(true);
        AGK_CORE_INFO("[WGCWindowCapture] Ready. Entering message loop.");

        MSG msg;
        while (!m_stopRequested.load() && GetMessage(&msg, nullptr, 0, 0)) {
            if (msg.message == WM_QUIT) {
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        AGK_CORE_INFO("[WGCWindowCapture] Message loop exited. Cleaning up WinRT session.");

        m_isInitialized.store(false);
        m_frameArrivedRevoker.revoke();
        if (m_captureSession) m_captureSession.Close();
        if (m_framePool) m_framePool.Close();
        
        m_captureSession = nullptr;
        m_framePool = nullptr;
        m_captureItem = nullptr;
        m_dispatcherController = nullptr;

        winrt::uninit_apartment();
    }

    bool WGCWindowCapture::InitWinRTSession(HWND hwnd) {
        try {
            auto factory = winrt::get_activation_factory<winrt::Windows::Graphics::Capture::GraphicsCaptureItem, IGraphicsCaptureItemInterop>();
            HRESULT hr;
            if (hwnd == GetDesktopWindow()) {
                HMONITOR hmon = MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY);
                hr = factory->CreateForMonitor(hmon, winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(), winrt::put_abi(m_captureItem));
            } else {
                hr = factory->CreateForWindow(hwnd, winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(), winrt::put_abi(m_captureItem));
            }
            if (FAILED(hr) || !m_captureItem) {
                AGK_CORE_ERROR("[WGCWindowCapture] Failed to create GraphicsCaptureItem for HWND {:p}", (void*)hwnd);
                return false;
            }

            m_width = m_captureItem.Size().Width;
            m_height = m_captureItem.Size().Height;
            if (m_width == 0 || m_height == 0) {
                AGK_CORE_ERROR("[WGCWindowCapture] Invalid capture item dimensions: {}x{}", m_width, m_height);
                return false;
            }

            // D3D11 Device
            D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
            hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                   D3D11_CREATE_DEVICE_BGRA_SUPPORT, 
                                   featureLevels, ARRAYSIZE(featureLevels), 
                                   D3D11_SDK_VERSION, &m_d3dDevice, nullptr, &m_d3dContext);
            if (FAILED(hr)) return false;

            ComPtr<IDXGIDevice> dxgiDevice;
            hr = m_d3dDevice.As(&dxgiDevice);
            if (FAILED(hr)) return false;

            winrt::com_ptr<::IInspectable> inspectableDevice;
            hr = CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.Get(), inspectableDevice.put());
            if (FAILED(hr)) return false;

            auto winrtDevice = inspectableDevice.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();

            m_framePool = winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::CreateFreeThreaded(
                winrtDevice, 
                winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized, 
                2, 
                m_captureItem.Size());

            m_frameArrivedRevoker = m_framePool.FrameArrived(winrt::auto_revoke, [this](auto const& sender, auto const&) {
                static int cbCount = 0;
                if (cbCount++ < 5) AGK_CORE_INFO("[WGCWindowCapture] FrameArrived Triggered ({})", cbCount);

                if (auto frame = sender.TryGetNextFrame()) {
                    std::lock_guard lock(m_frameMutex);
                    m_currentFrame = frame;
                    m_newFrameAvailable.store(true);
                }
            });

            m_captureSession = m_framePool.CreateCaptureSession(m_captureItem);
            
            try {
                if (winrt::Windows::Foundation::Metadata::ApiInformation::IsPropertyPresent(winrt::name_of<winrt::Windows::Graphics::Capture::GraphicsCaptureSession>(), L"IsCursorCaptureEnabled")) {
                     m_captureSession.IsCursorCaptureEnabled(true);
                }
                if (winrt::Windows::Foundation::Metadata::ApiInformation::IsPropertyPresent(winrt::name_of<winrt::Windows::Graphics::Capture::GraphicsCaptureSession>(), L"IsBorderRequired")) {
                     m_captureSession.IsBorderRequired(false);
                }
            } catch (...) {}

            if (!InitStagingTexture()) return false;

            m_captureSession.StartCapture();
            return true;

        } catch (winrt::hresult_error const& ex) {
            AGK_CORE_ERROR("[WGCWindowCapture] winrt Error: {}", winrt::to_string(ex.message()));
            return false;
        }
    }

    bool WGCWindowCapture::InitStagingTexture() {
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
            AGK_CORE_ERROR("[WGCWindowCapture] Failed to create staging texture: {:08x}", (uint32_t)hr);
            return false;
        }
        return true;
    }

    std::unique_ptr<CaptureFrame> WGCWindowCapture::AcquireNextFrame(uint32_t timeoutMs) {
        if (!m_isInitialized.load()) return nullptr;
        if (m_hasFrameAcquired) ReleaseFrame();

        auto start = std::chrono::steady_clock::now();
        while (!m_newFrameAvailable.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
            if (elapsed > timeoutMs) {
                static int tOutCount = 0;
                if (tOutCount++ < 10) AGK_CORE_WARN("[WGCWindowCapture] AcquireNextFrame timeout ({}ms). Total: {}", timeoutMs, tOutCount);
                return nullptr;
            }
        }

        winrt::Windows::Graphics::Capture::Direct3D11CaptureFrame frame{ nullptr };
        {
            std::lock_guard lock(m_frameMutex);
            frame = m_currentFrame;
            m_newFrameAvailable.store(false);
        }

        if (!frame) return nullptr;

        try {
            auto surface = frame.Surface();
            auto gpuTexture = GetDXGIInterface<ID3D11Texture2D>(surface);

            D3D11_TEXTURE2D_DESC gpuDesc;
            gpuTexture->GetDesc(&gpuDesc);
            if (gpuDesc.Width != (UINT)m_width || gpuDesc.Height != (UINT)m_height) {
                m_width = gpuDesc.Width;
                m_height = gpuDesc.Height;
                InitStagingTexture();
            }

            m_d3dContext->CopyResource(m_stagingTexture.Get(), gpuTexture.get());

            if (FAILED(m_d3dContext->Map(m_stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &m_mappedResource))) {
                return nullptr;
            }

            auto result = std::make_unique<CaptureFrame>();
            result->data = static_cast<const uint8_t*>(m_mappedResource.pData);
            result->linesize = m_mappedResource.RowPitch;
            result->width = m_width;
            result->height = m_height;
            result->timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_startTime).count();

            m_hasFrameAcquired = true;
            return result;
        } catch (winrt::hresult_error const& ex) {
            AGK_CORE_ERROR("[WGCWindowCapture] AcquireNextFrame failed (WinRT exception): {}", winrt::to_string(ex.message()));
            return nullptr;
        } catch (const std::exception& ex) {
            AGK_CORE_ERROR("[WGCWindowCapture] AcquireNextFrame failed (std::exception): {}", ex.what());
            return nullptr;
        } catch (...) {
            AGK_CORE_ERROR("[WGCWindowCapture] AcquireNextFrame failed (Unknown exception)");
            return nullptr;
        }
    }

    void WGCWindowCapture::ReleaseFrame() {
        if (m_hasFrameAcquired && m_stagingTexture) {
            m_d3dContext->Unmap(m_stagingTexture.Get(), 0);
            m_hasFrameAcquired = false;
        }
    }

} // namespace agk
