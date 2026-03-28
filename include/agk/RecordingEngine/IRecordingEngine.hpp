#pragma once

/**
 * @file IRecordingEngine.hpp
 * @brief Public interface for the agk RecordingEngine.
 */

namespace agk {

    /**
     * @brief Interface for the RecordingEngine.
     * 
     * This class defines the public API for the recording process,
     * abstracting away the implementation details of DXGI, WASAPI, and FFmpeg.
     */
    class IRecordingEngine {
    public:
        virtual ~IRecordingEngine() = default;

        /**
         * @brief Initialize the engine with the default configuration.
         * @return true if initialization was successful, false otherwise.
         */
        virtual bool Initialize() = 0;

        /**
         * @brief Start the recording process.
         */
        virtual void Start() = 0;

        /**
         * @brief Stop the recording process.
         */
        virtual void Stop() = 0;

        /**
         * @brief Pause the recording process.
         */
        virtual void Pause() = 0;

        /**
         * @brief Resume the recording process.
         */
        virtual void Resume() = 0;
    };

} // namespace agk
