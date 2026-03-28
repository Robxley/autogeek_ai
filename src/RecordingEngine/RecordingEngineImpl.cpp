/**
 * @file RecordingEngineImpl.cpp
 * @brief Implementation of the agk RecordingEngine interface.
 */

module;
#include <agk/RecordingEngine/Log.hpp>
#include "DXGIMonitorCapture.hpp"
#include "WGCWindowCapture.hpp"
#include "EncoderModule.hpp"
#include "SyncSystem.hpp"
#include <agk/RecordingEngine/ConfigSystem.hpp>
#include "RawInputModule.hpp"
#include "EventSerializer.hpp"
#include "TargetTracker.hpp"
#include "SessionManager.hpp"
#include "WASAPICapture.hpp"
extern "C" {
#include <libswscale/swscale.h>
}
#include <thread>
#include <atomic>
#include <filesystem>

module RecordingEngine;

import <memory>;

namespace agk {

    class RecordingEngineImpl : public IRecordingEngine {
    public:
        RecordingEngineImpl() : m_isRunning(false), m_isPaused(false), m_currentFrameIndex(0) {
            m_configSys = std::make_unique<ConfigSystem>();
            m_config = &m_configSys->GetConfig();
        }

        ~RecordingEngineImpl() override {
            Stop();
        }

        bool Initialize(const std::string& configPath = "") override {
            AGK_CORE_INFO("[Engine] Initializing sub-systems...");
            
            if (!configPath.empty()) {
                if (m_configSys->LoadFromFile(configPath)) {
                    AGK_CORE_INFO("[Engine] Loaded configuration from: {}", configPath);
                    agk::Log::AddFileSink(m_config->system.log_directory);
                } else {
                    AGK_CORE_WARN("[Engine] Failed to load config from {}, using defaults.", configPath);
                }
            }

            if (m_config->target.mode == "monitor" || m_config->target.mode == "monitor_crop") {
                auto capture = std::make_unique<DXGIMonitorCapture>();
                if (!capture->Initialize(m_config->target.monitor_index)) {
                     AGK_CORE_ERROR("[Engine] Failed to initialize DXGIMonitorCapture");
                     return false;
                }
                m_capture = std::move(capture);
            } else {
                AGK_CORE_INFO("[Engine] Capture mode set to 'window'. Targeted window will be initialized on Start().");
            }

            // Setup Input callback
            m_rawInput.Initialize([this](const InputEvent& event) {
                if (m_isRunning && !m_isPaused) {
                    InputEvent trackedEvent = event;
                    trackedEvent.timestamp = m_sync.GetRelativeTimeMs();
                    trackedEvent.frameIndex = m_currentFrameIndex.load();
                    m_serializer.SerializeEvent(trackedEvent);
                }
            }, &m_targetTracker);

            m_sync.Start();
            return true;
        }

        void Start() override {
            if (m_isRunning) return;

            AGK_CORE_INFO("[Engine] START sequence: mode='{}', process='{}', window='{}'", 
                m_config->target.mode, m_config->target.process_name, m_config->target.window_title);

            // Step 1: Initialize Capture based on mode
            if (m_config->target.mode == "window") {
                if (!m_targetTracker.Update(m_config->target.process_name, m_config->target.window_title)) {
                    AGK_CORE_ERROR("[Engine] Target process/window not found. Aborting start.");
                    return;
                }
            } else if (m_config->target.mode == "monitor_crop") {
                if (!m_targetTracker.Update(m_config->target.process_name, m_config->target.window_title)) {
                    AGK_CORE_WARN("[Engine] Target window not found for monitor_crop. Falling back to full monitor capture without crop.");
                }
            }

            if (m_config->target.mode == "window") {
                auto info = m_targetTracker.GetInfo();
                auto capture = std::make_unique<WGCWindowCapture>();
                if (!capture->Initialize(info.hwnd)) {
                    AGK_CORE_ERROR("[Engine] Failed to initialize WGCWindowCapture for targeted window.");
                    return;
                }
                m_capture = std::move(capture);
            } else if (!m_capture || !m_capture->IsInitialized()) {
                auto dxgi = std::make_unique<DXGIMonitorCapture>();
                if (dxgi->Initialize(m_config->target.monitor_index)) {
                    m_capture = std::move(dxgi);
                } else {
                    AGK_CORE_WARN("[Engine] DXGIMonitorCapture failed. Attempting robust fallback to WGCWindowCapture (Desktop).");
                    auto wgc = std::make_unique<WGCWindowCapture>();
                    if (wgc->Initialize(GetDesktopWindow())) {
                        m_capture = std::move(wgc);
                    } else {
                        AGK_CORE_ERROR("[Engine] Severe Error: All monitor capture backends failed.");
                        return; // Abort cleanly
                    }
                }
            }

            // Step 2: Create session via SessionManager
            if (!m_sessionManager.CreateSession(m_config->storage.base_output_path, *m_config)) {
                AGK_CORE_ERROR("[Engine] Failed to create session directory");
                return;
            }

            // Step 2: Initialize Audio (before Encoder)
            bool audioOk = false;
            if (m_config->audio.enabled) {
                // Initial target search to get PID
                m_targetTracker.Update(m_config->target.process_name, m_config->target.window_title);
                DWORD pid = m_config->audio.is_process_isolated ? m_targetTracker.GetInfo().processId : 0;
                
                audioOk = m_audioCapture.Initialize(m_config->audio, [this](const AudioBuffer& buffer) {
                    if (m_isRunning && !m_isPaused) {
                        m_encoder.EncodeAudioBuffer(buffer.data.data(), buffer.sampleCount, buffer.timestamp);
                    }
                }, pid);
            }

            std::string sessionPath = m_sessionManager.GetSessionPath();
            std::string videoPath = sessionPath + "/video.mkv";
            std::string eventsPath = sessionPath + "/" + m_config->storage.events_filename;

            if (!m_encoder.Initialize(videoPath, m_config->video, m_config->audio, audioOk ? m_audioCapture.GetFormat() : nullptr)) {
                AGK_CORE_ERROR("[Engine] Failed to initialize EncoderModule");
                return;
            }

            if (!m_serializer.Open(eventsPath)) {
                AGK_CORE_ERROR("[Engine] Failed to open events file: {}", eventsPath);
            }

            m_isRunning = true;
            m_isPaused = false;
            m_currentFrameIndex = 0;
            m_sync.Start();
            
            m_rawInput.Start();
            if (audioOk) m_audioCapture.Start();
            
            m_recordingThread = std::thread(&RecordingEngineImpl::RecordingLoop, this);
            
            AGK_CORE_INFO("[Engine] Recording started (Video + Audio + Inputs).");
        }

        void Stop() override {
            if (!m_isRunning) return;

            m_isRunning = false;
            m_rawInput.Stop();
            m_audioCapture.Stop();

            if (m_recordingThread.joinable()) {
                m_recordingThread.join();
            }

            m_encoder.Finalize();
            m_serializer.Close();
            
            AGK_CORE_INFO("[Engine] Recording stopped and finalized.");
        }

        void Pause() override {
            if (!m_isRunning || m_isPaused) return;
            m_isPaused = true;
            m_sync.Pause();
            AGK_CORE_INFO("[Engine] Recording paused.");
        }

        void Resume() override {
            if (!m_isRunning || !m_isPaused) return;
            m_isPaused = false;
            m_sync.Resume();
            AGK_CORE_INFO("[Engine] Recording resumed.");
        }

        void SetPreviewCallback(PreviewCallback callback, int width, int height) override {
            m_previewCallback = callback;
            m_previewWidth = width;
            m_previewHeight = height;
            AGK_CORE_INFO("[Engine] Preview callback set ({}x{}).", width, height);
        }

        void SetTargetWindow(const std::string& windowTitle) override {
            m_configSys->UpdateTargetWindow(windowTitle);
        }

        void SetTargetProcess(const std::string& processName) override {
            m_configSys->UpdateTargetProcess(processName);
        }

        void SetCaptureMode(const std::string& mode) override {
            m_configSys->UpdateTargetMode(mode);
        }

        const Config& GetConfig() const override {
            return m_configSys->GetConfig();
        }

        bool LoadConfig(const std::string& path) override {
            return m_configSys->LoadFromFile(path);
        }

        bool SaveConfig(const std::string& path) override {
            return m_configSys->SaveToFile(path);
        }

    private:
        void RecordingLoop() {
            try {
                winrt::init_apartment(winrt::apartment_type::multi_threaded);
            } catch (...) {}

            AGK_CORE_INFO("[Engine] Entering recording loop...");
            
            const double frameDuration = 1.0 / m_config->video.target_fps;
            int64_t frameIndex = 0;

            while (m_isRunning) {
                if (m_isPaused) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }

                auto startTime = std::chrono::steady_clock::now();

                // Periodically update target tracking (every 2 seconds @ 60fps)
                if (frameIndex % 120 == 0) {
                    m_targetTracker.Update(m_config->target.process_name, m_config->target.window_title);
                }

                // Capture Frame
                if (m_capture) {
                    if (auto frame = m_capture->AcquireNextFrame()) {
                        m_currentFrameIndex.store(frameIndex);
                        
                        int cropX = 0, cropY = 0, cropW = 0, cropH = 0;
                        if (m_config->target.mode == "monitor_crop") {
                            auto info = m_targetTracker.GetInfo();
                            cropX = (std::max)(0L, info.clientRect.left);
                            cropY = (std::max)(0L, info.clientRect.top);
                            cropW = (std::max)(0L, info.clientRect.right - info.clientRect.left);
                            cropH = (std::max)(0L, info.clientRect.bottom - info.clientRect.top);

                            if (cropX + cropW > frame->width) cropW = frame->width - cropX;
                            if (cropY + cropH > frame->height) cropH = frame->height - cropY;
                            
                            if (cropW % 2 != 0) cropW--;
                            if (cropH % 2 != 0) cropH--;
                            if (cropW <= 0 || cropH <= 0) {
                                cropX = 0; cropY = 0; cropW = 0; cropH = 0;
                            }
                        }
                        
                        m_encoder.EncodeVideoFrame(frame->data, frame->linesize, frameIndex++, cropX, cropY, cropW, cropH);

                        if (m_previewCallback) {
                            UpdatePreview(frame.get(), frameIndex);
                        }
                    }
                }

                // Wait for next frame (poor man's sync for now)
                auto endTime = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration<double>(endTime - startTime).count();
                double waitTime = frameDuration - elapsed;

                if (waitTime > 0) {
                    std::this_thread::sleep_for(std::chrono::duration<double>(waitTime));
                }
            }

            AGK_CORE_INFO("[Engine] Exiting recording loop.");
            if (m_swsPreviewCtx) {
                sws_freeContext(m_swsPreviewCtx);
                m_swsPreviewCtx = nullptr;
            }
        }

        void UpdatePreview(const CaptureFrame* decodedFrame, int64_t frameIndex) {
            // Re-allocate context only if source or destination dimensions change
            if (!m_swsPreviewCtx || 
                m_lastSourceWidth != decodedFrame->width || 
                m_lastSourceHeight != decodedFrame->height ||
                m_lastPreviewWidth != m_previewWidth || 
                m_lastPreviewHeight != m_previewHeight) {
                
                if (m_swsPreviewCtx) sws_freeContext(m_swsPreviewCtx);
                m_swsPreviewCtx = sws_getContext(decodedFrame->width, decodedFrame->height, AV_PIX_FMT_BGRA,
                                                m_previewWidth, m_previewHeight, AV_PIX_FMT_BGRA,
                                                SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
                
                m_lastSourceWidth = decodedFrame->width;
                m_lastSourceHeight = decodedFrame->height;
                m_lastPreviewWidth = m_previewWidth;
                m_lastPreviewHeight = m_previewHeight;
                m_previewBuffer.resize(m_previewWidth * m_previewHeight * 4);
            }

            uint8_t* dest[1] = {m_previewBuffer.data()};
            int destLinesize[1] = {m_previewWidth * 4};

            sws_scale(m_swsPreviewCtx, &decodedFrame->data, &decodedFrame->linesize, 0, decodedFrame->height, dest, destLinesize);

            PreviewFrame preview;
            preview.data = m_previewBuffer.data();
            preview.width = m_previewWidth;
            preview.height = m_previewHeight;
            preview.linesize = m_previewWidth * 4;
            preview.timestamp = m_sync.GetRelativeTimeMs();
            
            m_previewCallback(preview);
        }

        std::unique_ptr<ConfigSystem> m_configSys;
        const Config* m_config;
        std::unique_ptr<IFrameProvider> m_capture;
        EncoderModule m_encoder;
        SyncSystem m_sync;
        RawInputModule m_rawInput;
        EventSerializer m_serializer;
        TargetTracker m_targetTracker;
        SessionManager m_sessionManager;
        WASAPICapture m_audioCapture;
        
        std::atomic<bool> m_isRunning;
        std::atomic<bool> m_isPaused;
        std::atomic<int64_t> m_currentFrameIndex;
        std::thread m_recordingThread;

        // Preview
        PreviewCallback m_previewCallback;
        int m_previewWidth = 640;
        int m_previewHeight = 360;
        int m_lastSourceWidth = 0;
        int m_lastSourceHeight = 0;
        int m_lastPreviewWidth = 0;
        int m_lastPreviewHeight = 0;
        SwsContext* m_swsPreviewCtx = nullptr;
        std::vector<uint8_t> m_previewBuffer;
    };

    /**
     * @brief Factory function for creating the engine.
     */
    std::shared_ptr<IRecordingEngine> CreateEngine() {
        if (!agk::Log::GetCoreLogger()) {
            agk::Log::Init();
        }
        return std::make_shared<RecordingEngineImpl>();
    }

    /**
     * @brief Simple test function for module verification.
     */
    void TestRecordingEngine() {
        if (!agk::Log::GetCoreLogger()) {
            agk::Log::Init();
        }
        AGK_CORE_INFO("RecordingEngine module loaded successfully (Implementation)!");
        auto engine = CreateEngine();
        engine->Initialize();
    }

} // namespace agk
