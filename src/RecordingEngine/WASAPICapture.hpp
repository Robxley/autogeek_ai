#pragma once

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <wrl/client.h>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include "ConfigSystem.hpp"

namespace agk {

    using Microsoft::WRL::ComPtr;

    /**
     * @brief DTO for captured audio samples.
     */
    struct AudioBuffer {
        std::vector<uint8_t> data;
        uint32_t sampleCount = 0;
        uint64_t timestamp = 0;
    };

    /**
     * @brief System responsible for audio capture via WASAPI Loopback.
     */
    class WASAPICapture {
    public:
        using AudioCallback = std::function<void(const AudioBuffer&)>;

        WASAPICapture();
        ~WASAPICapture();

        /**
         * @brief Initializes the audio client.
         * @param config Audio configuration.
         * @param callback Function to call when new audio data is available.
         * @param processId Process ID for isolated capture (0 for global).
         * @return True if initialization succeeded.
         */
        bool Initialize(const AudioConfig& config, AudioCallback callback, DWORD processId = 0);

        void Start();
        void Stop();

        WAVEFORMATEX* GetFormat() const { return m_pwfx; }

    private:
        void CaptureThread();

        AudioConfig m_config;
        AudioCallback m_callback;
        DWORD m_targetProcessId = 0;

        ComPtr<IAudioClient> m_audioClient;
        ComPtr<IAudioCaptureClient> m_captureClient;
        WAVEFORMATEX* m_pwfx = nullptr;
        
        std::atomic<bool> m_isRunning;
        std::thread m_thread;
        HANDLE m_audioEvent = NULL;
    };

} // namespace agk
