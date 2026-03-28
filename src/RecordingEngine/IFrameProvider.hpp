#pragma once

#include <stdint.h>
#include <memory>

namespace agk {

    /**
     * @brief Represents a captured raw video frame from the GPU.
     */
    struct CaptureFrame {
        const uint8_t* data = nullptr;
        int linesize = 0;
        int width = 0;
        int height = 0;
        uint64_t timestamp = 0; // Relative timestamp in ms
    };

    /**
     * @brief Unified interface for video frame acquisition modules (DXGI, WGC, etc.)
     */
    class IFrameProvider {
    public:
        virtual ~IFrameProvider() = default;

        /**
         * @brief Checks if the capture system is properly initialized.
         */
        virtual bool IsInitialized() const = 0;

        /**
         * @brief Acquires the next available frame.
         * @param timeoutMs Maximum time to wait for a frame before returning nullptr.
         * @return A unique pointer to the CaptureFrame, or nullptr if timeout/error.
         */
        virtual std::unique_ptr<CaptureFrame> AcquireNextFrame(uint32_t timeoutMs = 100) = 0;

        /**
         * @brief Halts the capture system and releases tracking resources.
         */
        virtual void Stop() = 0;
    };

} // namespace agk
