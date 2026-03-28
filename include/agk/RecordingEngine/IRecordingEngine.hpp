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
        virtual void Stop() = 0;
        virtual void Pause() = 0;
        virtual void Resume() = 0;

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
