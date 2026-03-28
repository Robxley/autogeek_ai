#pragma once

#include <functional>
#include <cstdint>
#include <memory>
#include <string>

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

        virtual bool Initialize() = 0;
        virtual void Start() = 0;
        virtual void Stop() = 0;
        virtual void Pause() = 0;
        virtual void Resume() = 0;

        /**
         * @brief Sets a callback for live preview (monitoring).
         */
        virtual void SetPreviewCallback(PreviewCallback callback, int width, int height) = 0;
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
