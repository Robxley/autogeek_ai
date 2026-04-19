#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <queue>
#include <thread>
#include <condition_variable>
#include <vector>
#include <nlohmann/json.hpp>
#include "RawInputModule.hpp"

namespace agk {

    /**
     * @brief Serializes InputEvent objects into a JSONL (JSON Lines) file.
     * Groups events by frame_index and writes asynchronously for performance.
     */
    class EventSerializer {
    public:
        EventSerializer();
        ~EventSerializer();

        bool Open(const std::string& filename);
        void Close();

        /**
         * @brief Buffers an event. If the frame_index changes, the previous frame is flushed to the async queue.
         */
        void PushEvent(const InputEvent& event);

    private:
        void WorkerLoop();
        void FlushCurrentFrame();

        struct FrameData {
            int64_t frameIndex = -1;
            struct TimestampGroup {
                uint64_t timeMs;
                std::vector<nlohmann::json> mouseEvents;
                std::vector<nlohmann::json> keyboardEvents;
                std::vector<nlohmann::json> gamepadEvents;
            };
            std::vector<TimestampGroup> groups;
        };

        std::ofstream m_file;
        bool m_isOpen;

        // Sync & Buffering
        std::mutex m_mutex;
        FrameData m_currentFrame;
        
        // Async Writer
        std::mutex m_queueMutex;
        std::condition_variable m_cv;
        std::queue<nlohmann::json> m_writeQueue;
        std::thread m_workerThread;
        std::atomic<bool> m_shouldExit;
    };

} // namespace agk
