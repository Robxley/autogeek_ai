/**
 * @file RecordingEngineImpl.cpp
 * @brief Implementation of the agk RecordingEngine interface.
 */

module;
#include "Log.hpp"
#include "DXGICapture.hpp"
#include "EncoderModule.hpp"
#include "SyncSystem.hpp"
#include "ConfigSystem.hpp"
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
            m_config = std::make_unique<Config>(); // Default config
        }

        ~RecordingEngineImpl() override {
            Stop();
        }

        bool Initialize() override {
            AGK_CORE_INFO("[Engine] Initializing sub-systems...");
            
            TargetTracker::EnableDpiAwareness();

            if (!m_capture.Initialize()) {
                AGK_CORE_ERROR("[Engine] Failed to initialize DXGICapture");
                return false;
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

            // Step 1: Create session via SessionManager
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

    private:
        void RecordingLoop() {
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
                if (auto frame = m_capture.AcquireNextFrame()) {
                    // Update atomic index for input synchronization
                    m_currentFrameIndex.store(frameIndex);
                    
                    // Encode Frame
                    m_encoder.EncodeVideoFrame(frame->data, frame->linesize, frameIndex++);

                    // Preview Support (Live Monitoring)
                    if (m_previewCallback) {
                        UpdatePreview(frame.get(), frameIndex);
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
            if (!m_swsPreviewCtx || m_lastPreviewWidth != m_previewWidth || m_lastPreviewHeight != m_previewHeight) {
                if (m_swsPreviewCtx) sws_freeContext(m_swsPreviewCtx);
                m_swsPreviewCtx = sws_getContext(decodedFrame->width, decodedFrame->height, AV_PIX_FMT_BGRA,
                                                m_previewWidth, m_previewHeight, AV_PIX_FMT_BGRA,
                                                SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
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

        std::unique_ptr<Config> m_config;
        DXGICapture m_capture;
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
