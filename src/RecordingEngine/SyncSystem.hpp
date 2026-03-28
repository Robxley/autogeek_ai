#pragma once

#include <chrono>

namespace agk {

    /**
     * @brief Manages the timeline of the recording with pause/resume support.
     */
    class SyncSystem {
    public:
        SyncSystem() = default;

        /**
         * @brief Starts the clock for the recording.
         */
        void Start() {
            m_startTime = std::chrono::steady_clock::now();
            m_accumulatedTime = 0.0;
            m_paused = false;
        }

        /**
         * @brief Pauses the clock. Accumulated time is updated.
         */
        void Pause() {
            if (!m_paused) {
                m_accumulatedTime += GetCurrentSegmentMs();
                m_paused = true;
            }
        }

        /**
         * @brief Resumes the clock from a pause.
         */
        void Resume() {
            if (m_paused) {
                m_startTime = std::chrono::steady_clock::now();
                m_paused = false;
            }
        }

        /**
         * @brief Gets the total relative time in milliseconds since Start, excluding pauses.
         */
        double GetRelativeTimeMs() const {
            if (m_paused) {
                return m_accumulatedTime;
            }
            return m_accumulatedTime + GetCurrentSegmentMs();
        }

        /**
         * @brief Gets the current frame index based on target FPS.
         */
        uint64_t GetFrameIndex(int targetFps) const {
            return static_cast<uint64_t>((GetRelativeTimeMs() / 1000.0) * targetFps);
        }

    private:
        double GetCurrentSegmentMs() const {
            auto now = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - m_startTime);
            return static_cast<double>(duration.count()) / 1000.0;
        }

        std::chrono::steady_clock::time_point m_startTime;
        double m_accumulatedTime = 0.0;
        bool m_paused = false;
    };

} // namespace agk
