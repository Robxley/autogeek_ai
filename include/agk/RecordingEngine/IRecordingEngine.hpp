#pragma once

#include <functional>
#include <cstdint>
#include <memory>
#include <string>
#include <agk/RecordingEngine/ConfigSystem.hpp> // Required for Config reference

namespace agk {

    /**
     * @brief Structure for a downscaled preview frame (Live Monitoring).
     */
    struct PreviewFrame {
        const uint8_t* data;
        int width;
        int height;
        int linesize;
        uint64_t timestamp;
    };

    /**
     * @brief Detailed dynamic engine statistics.
     */
    struct EngineStats {
        uint64_t recordingTimeMs = 0;
        uint64_t framesCaptured = 0;
        uint64_t framesDropped = 0;
        float currentFPS = 0.0f; // Live FPS
        float audioLevelRMS = 0.0f; // Silent preview peak meter
        float mouseDeltaActivity = 0.0f; // Distance moved dynamically
        float keyboardActivityLevel = 0.0f; // Keys pressed dynamically
        std::string pauseReason = ""; // Empty if running, contains reason if paused
    };

    /**
     * @brief Current target info (for Preview mode helpers).
     */
    struct TargetState {
        std::string processName = "";
        std::string windowTitle = "";
    };

    /**
     * @brief Callback type for receiving live preview frames.
     */
    using PreviewCallback = std::function<void(const PreviewFrame&)>;

    /**
     * @brief Interface for the RecordingEngine.
     */
    class IRecordingEngine {
    public:
        virtual ~IRecordingEngine() = default;

        virtual bool Initialize(const std::string& configPath = "") = 0;
        virtual void Start() = 0;
        virtual void StartPreview() = 0; // Distinct silent preview
        virtual void Stop() = 0;
        virtual void Pause() = 0;
        virtual void Resume() = 0;
        
        virtual bool IsRecording() const = 0;
        virtual bool IsPreviewing() const = 0;
        virtual EngineStats GetStats() const = 0;
        virtual TargetState GetTargetState() const = 0;

        /**
         * @brief Sets a callback for live preview (monitoring).
         */
        virtual void SetPreviewCallback(PreviewCallback callback, int width, int height) = 0;

        /**
         * @brief Sets the target window title for "window" or "monitor_crop" mode.
         */
        virtual void SetTargetWindow(const std::string& windowTitle) = 0;

        /**
         * @brief Sets the target process name for "process" mode.
         */
        virtual void SetTargetProcess(const std::string& processName) = 0;

        /**
         * @brief Dynamically changes the capture mode ("monitor", "window", or "monitor_crop").
         */
        virtual void SetCaptureMode(const std::string& mode) = 0;

        /**
         * @brief Returns a reference to the active configuration.
         */
        virtual const Config& GetConfig() const = 0;

        /**
         * @brief Returns a mutable reference to the active configuration.
         */
        virtual Config& GetMutableConfig() = 0;

        /**
         * @brief Loads engine configuration from a file.
         */
        virtual bool LoadConfig(const std::string& path) = 0;

        /**
         * @brief Saves current engine configuration to a file.
         */
        virtual bool SaveConfig(const std::string& path) = 0;
    };

    /**
     * @brief Factory function for creating the engine.
     */
    std::shared_ptr<IRecordingEngine> CreateEngine();

    /**
     * @brief Simple test function for module verification.
     */
    void TestRecordingEngine();

} // namespace agk
