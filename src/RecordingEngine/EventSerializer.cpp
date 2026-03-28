#include "EventSerializer.hpp"
#include <iostream>

using json = nlohmann::json;

namespace agk {

    EventSerializer::EventSerializer() : m_isOpen(false) {}

    EventSerializer::~EventSerializer() {
        Close();
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
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_isOpen) {
            m_file.close();
            m_isOpen = false;
        }
    }

    void EventSerializer::SerializeEvent(const InputEvent& event) {
        if (!m_isOpen) return;

        json j;
        switch (event.type) {
            case InputType::KeyDown:   j["type"] = "KEYDOWN"; break;
            case InputType::KeyUp:     j["type"] = "KEYUP"; break;
            case InputType::MouseDown: j["type"] = "MOUSEDOWN"; break;
            case InputType::MouseUp:   j["type"] = "MOUSEUP"; break;
            case InputType::MouseMove: j["type"] = "MOUSEMOVE"; break;
            case InputType::MouseWheel:j["type"] = "MOUSEWHEEL"; break;
        }

        j["timestamp"] = event.timestamp;
        j["frame_index"] = event.frameIndex;
        
        if (event.type == InputType::KeyDown || event.type == InputType::KeyUp) {
            j["vk"] = event.vkCode;
        } else if (event.type == InputType::MouseMove) {
            j["x"] = event.x;
            j["y"] = event.y;
        } else if (event.type == InputType::MouseDown || event.type == InputType::MouseUp) {
            j["button"] = event.button;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        m_file << j.dump() << std::endl;
    }

} // namespace agk
