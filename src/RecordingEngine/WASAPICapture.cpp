#include "WASAPICapture.hpp"
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <iostream>
#include "Log.hpp"

namespace agk {

    WASAPICapture::WASAPICapture() : m_pwfx(nullptr), m_isRunning(false), m_targetProcessId(0), m_audioEvent(NULL) {}

    WASAPICapture::~WASAPICapture() {
        Stop();
        if (m_pwfx) {
            CoTaskMemFree(m_pwfx);
        }
    }

    bool WASAPICapture::Initialize(const AudioConfig& config, AudioCallback callback, DWORD processId) {
        m_config = config;
        m_callback = callback;
        m_targetProcessId = processId;

        HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
            AGK_CORE_ERROR("[WASAPI] Failed to initialize COM: 0x{:08X}", (uint32_t)hr);
            return false;
        }

        ComPtr<IMMDeviceEnumerator> enumerator;
        hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&enumerator);
        if (FAILED(hr)) {
            AGK_CORE_ERROR("[WASAPI] Failed to create MMDeviceEnumerator: 0x{:08X}", (uint32_t)hr);
            return false;
        }

        ComPtr<IMMDevice> device;
        hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
        if (FAILED(hr)) {
            AGK_CORE_ERROR("[WASAPI] Failed to get default audio endpoint: 0x{:08X}", (uint32_t)hr);
            return false;
        }

        hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&m_audioClient);
        if (FAILED(hr)) {
            AGK_CORE_ERROR("[WASAPI] Failed to activate IAudioClient: 0x{:08X}", (uint32_t)hr);
            return false;
        }

        hr = m_audioClient->GetMixFormat(&m_pwfx);
        if (FAILED(hr)) {
            AGK_CORE_ERROR("[WASAPI] Failed to get mix format: 0x{:08X}", (uint32_t)hr);
            return false;
        }

        // Process Loopback integration (Windows 10 21H1+)
        DWORD streamFlags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
        
        // Flag for process-isolated loopback (Win10 build 20348+)
        // #define AUDCLNT_STREAMFLAGS_PROCESS_LOOPBACK 0x00200000
        const DWORD PROCESS_LOOPBACK_FLAG = 0x00200000;

        if (m_targetProcessId != 0 && m_config.is_process_isolated) {
            streamFlags |= PROCESS_LOOPBACK_FLAG;
            AGK_CORE_INFO("[WASAPI] Initializing Process-Isolated Loopback for PID: {}", m_targetProcessId);
            
            // For process loopback, we need to provide the PID via AUDCLNT_PROCESS_LOOPBACK_PARAMS
            // This usually requires IAudioClient3 or newer activation, but let's try the flag first.
            // Note: Official activation usually involves ActivateAudioInterfaceAsync.
        } else {
            streamFlags |= AUDCLNT_STREAMFLAGS_LOOPBACK;
            AGK_CORE_INFO("[WASAPI] Initializing Global Loopback.");
        }

        hr = m_audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, streamFlags, 0, 0, m_pwfx, (streamFlags & PROCESS_LOOPBACK_FLAG) ? (LPCGUID)&m_targetProcessId : NULL);
        if (FAILED(hr)) {
            AGK_CORE_ERROR("[WASAPI] Failed to initialize audio client: 0x{:08X}", (uint32_t)hr);
            return false;
        }

        m_audioEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        hr = m_audioClient->SetEventHandle(m_audioEvent);
        if (FAILED(hr)) {
            AGK_CORE_ERROR("[WASAPI] Failed to set event handle: 0x{:08X}", (uint32_t)hr);
            return false;
        }

        hr = m_audioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&m_captureClient);
        if (FAILED(hr)) {
            AGK_CORE_ERROR("[WASAPI] Failed to get IAudioCaptureClient: 0x{:08X}", (uint32_t)hr);
            return false;
        }

        AGK_CORE_INFO("[WASAPI] Audio capture initialized ({} Hz, {} channels).", m_pwfx->nSamplesPerSec, m_pwfx->nChannels);
        return true;
    }

    void WASAPICapture::Start() {
        if (m_isRunning) return;
        m_isRunning = true;
        m_audioClient->Start();
        m_thread = std::thread(&WASAPICapture::CaptureThread, this);
    }

    void WASAPICapture::Stop() {
        if (!m_isRunning) return;
        m_isRunning = false;
        
        if (m_audioEvent) SetEvent(m_audioEvent); // Wake up thread

        if (m_thread.joinable()) {
            m_thread.join();
        }

        if (m_audioClient) m_audioClient->Stop();
        if (m_audioEvent) {
            CloseHandle(m_audioEvent);
            m_audioEvent = NULL;
        }
    }

    void WASAPICapture::CaptureThread() {
        UINT32 packetSize = 0;
        HRESULT hr;

        AGK_CORE_INFO("[WASAPI] Capture thread started.");

        while (m_isRunning) {
            WaitForSingleObject(m_audioEvent, 1000);
            if (!m_isRunning) break;

            hr = m_captureClient->GetNextPacketSize(&packetSize);
            while (packetSize != 0) {
                BYTE* pData;
                UINT32 numFramesRead;
                DWORD flags;
                UINT64 devicePosition;
                UINT64 qpcPosition;

                hr = m_captureClient->GetBuffer(&pData, &numFramesRead, &flags, &devicePosition, &qpcPosition);
                if (SUCCEEDED(hr)) {
                    AudioBuffer buffer;
                    buffer.sampleCount = numFramesRead;
                    buffer.timestamp = qpcPosition; // Use QPC for raw timing
                    
                    int bytesPerFrame = m_pwfx->nBlockAlign;
                    buffer.data.assign(pData, pData + (numFramesRead * bytesPerFrame));

                    if (m_callback) {
                        m_callback(buffer);
                    }

                    m_captureClient->ReleaseBuffer(numFramesRead);
                    hr = m_captureClient->GetNextPacketSize(&packetSize);
                } else {
                    break;
                }
            }
        }

        AGK_CORE_INFO("[WASAPI] Capture thread stopped.");
    }

} // namespace agk
