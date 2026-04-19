#include "EventSerializer.hpp"
#include <agk/RecordingEngine/Log.hpp>
#include <iomanip>
#include <cmath>

using json = nlohmann::json;

namespace agk {

    EventSerializer::EventSerializer() : m_isOpen(false), m_shouldExit(false) {
        m_currentFrame.frameIndex = -1;
        m_workerThread = std::thread(&EventSerializer::WorkerLoop, this);
    }

    EventSerializer::~EventSerializer() {
        FlushCurrentFrame();
        m_shouldExit = true;
        m_cv.notify_all();
        if (m_workerThread.joinable()) {
            m_workerThread.join();
        }
        
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_file.is_open()) {
            m_file.close();
            m_isOpen = false;
        }
    }

    bool EventSerializer::Open(const std::string& filename) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_file.open(filename, std::ios::app);
        if (m_file.is_open()) {
            m_isOpen = true;
            return true;
        }
        return false;
    }

    void EventSerializer::Close() {
        FlushCurrentFrame();
        // We don't close here anymore, we let the destructor handle it after joining the worker.
        // This ensures all pending frames are written.
    }

    void EventSerializer::PushEvent(const InputEvent& event) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // If frame changed, flush previous frame to queue
        if (m_currentFrame.frameIndex != -1 && m_currentFrame.frameIndex != event.frameIndex) {
            FlushCurrentFrame();
        }
        
        if (m_currentFrame.frameIndex == -1) {
            m_currentFrame.frameIndex = event.frameIndex;
        }

        uint64_t timeMs = static_cast<uint64_t>(std::round(event.timestamp));
        
        // Find or create a group for this timestamp within the frame
        auto it = std::find_if(m_currentFrame.groups.begin(), m_currentFrame.groups.end(),
            [timeMs](const FrameData::TimestampGroup& g) { return g.timeMs == timeMs; });
        
        if (it == m_currentFrame.groups.end()) {
            m_currentFrame.groups.push_back({timeMs});
            it = std::prev(m_currentFrame.groups.end());
        }

        auto round3 = [](double v) { return std::round(v * 1000.0) / 1000.0; };

        json j;
        switch (event.type) {
            case InputType::KeyDown:
                j["type"] = "DOWN";
                j["vk"] = event.vkCode;
                it->keyboardEvents.push_back(std::move(j));
                break;
            case InputType::KeyUp:
                j["type"] = "UP";
                j["vk"] = event.vkCode;
                it->keyboardEvents.push_back(std::move(j));
                break;
            case InputType::MouseDown:
                j["type"] = "DOWN";
                j["button"] = event.button;
                j["x"] = round3(event.x);
                j["y"] = round3(event.y);
                it->mouseEvents.push_back(std::move(j));
                break;
            case InputType::MouseUp:
                j["type"] = "UP";
                j["button"] = event.button;
                j["x"] = round3(event.x);
                j["y"] = round3(event.y);
                it->mouseEvents.push_back(std::move(j));
                break;
            case InputType::MouseMove:
                j["type"] = "MOVE";
                j["x"] = round3(event.x);
                j["y"] = round3(event.y);
                j["dx"] = round3(event.dx);
                j["dy"] = round3(event.dy);
                it->mouseEvents.push_back(std::move(j));
                break;
            case InputType::MouseWheel:
                j["type"] = "WHEEL";
                j["delta"] = event.button; // reused field for delta
                it->mouseEvents.push_back(std::move(j));
                break;
        }
    }

    void EventSerializer::FlushCurrentFrame() {
        if (m_currentFrame.frameIndex == -1) return;

        json frame;
        frame["frame_index"] = m_currentFrame.frameIndex;
        json tags = json::array();

        for (auto& g : m_currentFrame.groups) {
            json group;
            group["time"] = g.timeMs;
            if (!g.mouseEvents.empty())    group["mouse"] = std::move(g.mouseEvents);
            if (!g.keyboardEvents.empty()) group["keyboard"] = std::move(g.keyboardEvents);
            if (!g.gamepadEvents.empty())  group["gamepad"] = std::move(g.gamepadEvents);
            tags.push_back(std::move(group));
        }
        frame["timestamps"] = std::move(tags);

        // Reset current frame state while still under m_mutex if called from PushEvent
        m_currentFrame.frameIndex = -1;
        m_currentFrame.groups.clear();

        {
            std::lock_guard<std::mutex> qLock(m_queueMutex);
            m_writeQueue.push(std::move(frame));
        }
        m_cv.notify_one();
    }

    void EventSerializer::WorkerLoop() {
        while (true) {
            json item;
            {
                std::unique_lock<std::mutex> lock(m_queueMutex);
                m_cv.wait(lock, [this] { return !m_writeQueue.empty() || m_shouldExit; });
                
                if (m_shouldExit && m_writeQueue.empty()) break;
                
                if (!m_writeQueue.empty()) {
                    item = std::move(m_writeQueue.front());
                    m_writeQueue.pop();
                } else {
                    continue;
                }
            }

            std::lock_guard<std::mutex> fileLock(m_mutex);
            if (m_file.is_open()) {
                m_file << item.dump() << std::endl;
                m_file.flush();
            }
        }
    }

} // namespace agk
