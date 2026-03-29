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
        RecordingEngineImpl() : m_isRunning(false), m_isPaused(false), m_isPreviewing(false), m_currentFrameIndex(0) {
            m_configSys = std::make_unique<ConfigSystem>();
            m_config = &m_configSys->GetConfig();
        }

        ~RecordingEngineImpl() override {
            Stop();
        }

        bool Initialize(const std::string& configPath = "") override {
            AGK_CORE_INFO("[Engine] Initializing sub-systems...");
            m_targetTracker.EnableDpiAwareness();
            
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
                    if (event.type == InputType::KeyDown) {
                        m_telemetryKeys++;
                    } else if (event.type == InputType::MouseMove) {
                        if (m_lastMouseX != -1 && m_lastMouseY != -1) {
                            double dx = event.x - m_lastMouseX;
                            double dy = event.y - m_lastMouseY;
                            m_telemetryMouseDistSq.fetch_add(static_cast<uint64_t>(dx * dx + dy * dy));
                        }
                        m_lastMouseX = static_cast<int>(event.x);
                        m_lastMouseY = static_cast<int>(event.y);
                    }

                    if (!m_isPreviewing) {
                        InputEvent trackedEvent = event;
                        trackedEvent.timestamp = m_sync.GetRelativeTimeMs();
                        trackedEvent.frameIndex = m_currentFrameIndex.load();
                        m_serializer.SerializeEvent(trackedEvent);
                    }
                }
            }, &m_targetTracker);

            m_sync.Start();
            return true;
        }

        void Start() override { InternalStart(false); }
        void StartPreview() override { InternalStart(true); }

        void InternalStart(bool previewOnly) {
            if (m_isRunning) return;

            m_isPreviewing = previewOnly;

            {
                std::lock_guard<std::mutex> lock(m_statsMutex);
                m_currentStats = EngineStats(); // Reset stats
                m_telemetryKeys = 0;
                m_telemetryMouseDistSq = 0;
                m_lastMouseX = -1;
                m_lastMouseY = -1;
                m_lastStatsTimeMs = 0;
                m_lastStatsFrameCount = 0;
            }

            AGK_CORE_INFO("[Engine] START sequence (Preview={}): mode='{}', monitor={}, process='{}', window='{}'", 
                previewOnly, m_config->target.mode, m_config->target.monitor_index, m_config->target.process_name, m_config->target.window_title);

            m_targetTracker.SetClientAreaOnly(m_config->target.client_area_only);

            // Step 1: Initialize Capture based on mode
            if (m_config->target.mode == "foreground") {
                HWND fg = GetForegroundWindow();
                if (fg) m_targetTracker.UpdateFromHWND(fg);
            } else if (m_config->target.mode == "window") {
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

            // Step 2: Create session via SessionManager ONLY if recording
            if (!previewOnly) {
                if (!m_sessionManager.CreateSession(m_config->storage.base_output_path, *m_config)) {
                    AGK_CORE_ERROR("[Engine] Failed to create session directory");
                    return;
                }
            }

            // Step 2: Initialize Audio (before Encoder)
            bool audioOk = false;
            if (m_config->audio.enabled) {
                // Initial target search to get PID
                m_targetTracker.Update(m_config->target.process_name, m_config->target.window_title);
                DWORD pid = m_config->audio.is_process_isolated ? m_targetTracker.GetInfo().processId : 0;
                
                audioOk = m_audioCapture.Initialize(m_config->audio, [this, previewOnly](const AudioBuffer& buffer) {
                    if (m_isRunning && !m_isPaused && !buffer.data.empty()) {
                        
                        // Calculate Peak Amplitude for VU Meter
                        float maxAmp = 0.0f;
                        for (float sample : buffer.data) {
                            maxAmp = (std::max)(maxAmp, std::abs(sample));
                        }
                        
                        {
                            std::lock_guard<std::mutex> lock(m_statsMutex);
                            m_currentStats.audioLevelRMS = maxAmp; // using peak mapped directly
                        }

                        if (!previewOnly) {
                            m_encoder.EncodeAudioBuffer(buffer.data.data(), buffer.sampleCount, buffer.timestamp);
                        }
                    }
                }, pid);
            }

            if (!previewOnly) {
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
            }

            m_isRunning = true;
            m_isPaused = false;
            m_currentFrameIndex = 0;
            m_sync.Start();
            
            if (!previewOnly) m_rawInput.Start();
            if (audioOk) m_audioCapture.Start();
            
            m_recordingThread = std::thread(&RecordingEngineImpl::RecordingLoop, this);
            
            AGK_CORE_INFO("[Engine] {} started.", previewOnly ? "Preview" : "Recording");
        }

        void Stop() override {
            if (!m_isRunning) return;

            m_isRunning = false;
            
            if (!m_isPreviewing) m_rawInput.Stop();
            
            m_audioCapture.Stop();

            if (m_recordingThread.joinable()) {
                m_recordingThread.join();
            }

            if (!m_isPreviewing) {
                m_encoder.Finalize();
                m_serializer.Close();
            }
            
            AGK_CORE_INFO("[Engine] {} stopped.", m_isPreviewing ? "Preview" : "Recording");
            
            m_isPreviewing = false;
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

        Config& GetMutableConfig() override {
            return m_configSys->GetMutableConfig();
        }

        bool LoadConfig(const std::string& path) override {
            return m_configSys->LoadFromFile(path);
        }

        bool SaveConfig(const std::string& path) override {
            return m_configSys->SaveToFile(path);
        }

        bool IsRecording() const override { return m_isRunning && !m_isPreviewing; }
        bool IsPreviewing() const override { return m_isRunning && m_isPreviewing; }
        
        EngineStats GetStats() const override {
            std::lock_guard<std::mutex> lock(m_statsMutex);
            EngineStats stats = m_currentStats;
            if (m_isRunning) {
                uint64_t nowMs = m_sync.GetRelativeTimeMs();
                stats.recordingTimeMs = nowMs;
                stats.framesCaptured = m_currentFrameIndex.load();
                
                uint64_t dt = nowMs - m_lastStatsTimeMs;
                if (dt >= 100) { // Re-compute running 10Hz sampling
                    uint64_t df = stats.framesCaptured - m_lastStatsFrameCount;
                    m_currentStats.currentFPS = (float)df / (dt / 1000.0f);
                    m_currentStats.keyboardActivityLevel = (float)m_telemetryKeys.exchange(0) / (dt / 1000.0f);
                    m_currentStats.mouseDeltaActivity = std::sqrt((float)m_telemetryMouseDistSq.exchange(0));
                    
                    m_lastStatsTimeMs = nowMs;
                    m_lastStatsFrameCount = stats.framesCaptured;
                }
                
                stats.currentFPS = m_currentStats.currentFPS;
                stats.keyboardActivityLevel = m_currentStats.keyboardActivityLevel;
                stats.mouseDeltaActivity = m_currentStats.mouseDeltaActivity;
            }
            return stats;
        }

        TargetState GetTargetState() const override {
            TargetState state;
            auto info = m_targetTracker.GetInfo();
            state.processName = m_targetTracker.GetSearchProcessName();
            char title[MAX_PATH] = {0};
            if (info.hwnd) GetWindowTextA(info.hwnd, title, MAX_PATH);
            state.windowTitle = title;
            return state;
        }

    private:
        void RecordingLoop() {
            try {
                winrt::init_apartment(winrt::apartment_type::multi_threaded);
            } catch (...) {}

            AGK_CORE_INFO("[Engine] Entering recording loop...");
            
            const double frameDuration = 1.0 / m_config->video.target_fps;
            int64_t frameIndex = 0;
            auto lastTrackTime = std::chrono::steady_clock::now();

            while (m_isRunning) {
                auto loopTime = std::chrono::steady_clock::now();

                // Periodically update target tracking (every 1 second) regardless of pause state
                if (std::chrono::duration_cast<std::chrono::milliseconds>(loopTime - lastTrackTime).count() >= 1000) {
                    lastTrackTime = loopTime;
                    
                    if (m_config->target.mode == "foreground") {
                        HWND fg = GetForegroundWindow();
                        DWORD fgPid = 0;
                        if (fg) GetWindowThreadProcessId(fg, &fgPid);
                        
                        // NOTE: GetCurrentProcessId() is TrackerStudio
                        if (fgPid != GetCurrentProcessId()) {
                            m_targetTracker.UpdateFromHWND(fg);
                            if (m_isPaused && m_config->target.auto_resume_on_restore) {
                                m_isPaused = false;
                                m_sync.Resume();
                                { std::lock_guard<std::mutex> lock(m_statsMutex); m_currentStats.pauseReason = ""; }
                                AGK_CORE_INFO("[Engine] Auto-Resumed (Focus shifted to external window).");
                            }
                        } else {
                            if (!m_isPaused && m_config->target.auto_pause_on_minimize) {
                                m_isPaused = true;
                                m_sync.Pause();
                                { std::lock_guard<std::mutex> lock(m_statsMutex); m_currentStats.pauseReason = "Studio gained focus"; }
                                AGK_CORE_INFO("[Engine] Auto-Paused (Studio gained focus).");
                            }
                        }
                    } else {
                        m_targetTracker.Update(m_config->target.process_name, m_config->target.window_title);
                    }
                }

                if (m_isPaused) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }

                auto startTime = std::chrono::steady_clock::now();

                // Capture Frame
                if (m_capture) {
                    if (auto frame = m_capture->AcquireNextFrame()) {
                        m_currentFrameIndex.store(frameIndex);
                        
                        int cropX = 0, cropY = 0, cropW = 0, cropH = 0;
                        if (m_config->target.mode == "monitor_crop" || m_config->target.mode == "foreground") {
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
                        
                        if (!m_isPreviewing) {
                            m_encoder.EncodeVideoFrame(frame->data, frame->linesize, frameIndex++, cropX, cropY, cropW, cropH);
                        } else {
                            frameIndex++;
                        }

                        if (m_previewCallback) {
                            UpdatePreview(frame.get(), cropX, cropY, cropW, cropH, frameIndex);
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

        void UpdatePreview(const CaptureFrame* decodedFrame, int cropX, int cropY, int cropW, int cropH, int64_t frameIndex) {
            int actualSourceWidth = (cropW > 0) ? cropW : decodedFrame->width;
            int actualSourceHeight = (cropH > 0) ? cropH : decodedFrame->height;
            const uint8_t* srcData = decodedFrame->data;
            
            if (cropW > 0 && cropH > 0) {
                srcData += (cropY * decodedFrame->linesize) + (cropX * 4); // BGRA offset
            }

            // Re-allocate context only if source or destination dimensions change
            if (!m_swsPreviewCtx || 
                m_lastSourceWidth != actualSourceWidth || 
                m_lastSourceHeight != actualSourceHeight ||
                m_lastPreviewWidth != m_previewWidth || 
                m_lastPreviewHeight != m_previewHeight) {
                
                if (m_swsPreviewCtx) sws_freeContext(m_swsPreviewCtx);
                m_swsPreviewCtx = sws_getContext(actualSourceWidth, actualSourceHeight, AV_PIX_FMT_BGRA,
                                                m_previewWidth, m_previewHeight, AV_PIX_FMT_BGRA,
                                                SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
                
                m_lastSourceWidth = actualSourceWidth;
                m_lastSourceHeight = actualSourceHeight;
                m_lastPreviewWidth = m_previewWidth;
                m_lastPreviewHeight = m_previewHeight;
                m_previewBuffer.resize(m_previewWidth * m_previewHeight * 4);
            }

            uint8_t* dest[1] = {m_previewBuffer.data()};
            int destLinesize[1] = {m_previewWidth * 4};

            uint8_t* srcArray[1] = {const_cast<uint8_t*>(srcData)};
            int srcLinesize[1] = {decodedFrame->linesize};

            sws_scale(m_swsPreviewCtx, srcArray, srcLinesize, 0, actualSourceHeight, dest, destLinesize);

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
        std::atomic<bool> m_isPreviewing;
        std::atomic<int64_t> m_currentFrameIndex;
        std::thread m_recordingThread;

        mutable EngineStats m_currentStats;
        mutable std::mutex m_statsMutex;
        
        // Telemetry accumulation
        mutable std::atomic<uint64_t> m_telemetryKeys{0};
        mutable std::atomic<uint64_t> m_telemetryMouseDistSq{0};
        mutable int m_lastMouseX = -1;
        mutable int m_lastMouseY = -1;
        mutable uint64_t m_lastStatsTimeMs = 0;
        mutable uint64_t m_lastStatsFrameCount = 0;

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
