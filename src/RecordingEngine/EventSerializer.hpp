#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <nlohmann/json.hpp>
#include "RawInputModule.hpp"

namespace agk {

    /**
     * @brief Serializes InputEvent objects into a JSONL (JSON Lines) file.
     */
    class EventSerializer {
    public:
        EventSerializer();
        ~EventSerializer();

        bool Open(const std::string& filename);
        void Close();

        void SerializeEvent(const InputEvent& event);

    private:
        std::ofstream m_file;
        std::mutex m_mutex;
        bool m_isOpen;
    };

} // namespace agk
