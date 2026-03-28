#pragma once

#include <spdlog/sinks/base_sink.h>
#include <spdlog/details/null_mutex.h>
#include <mutex>
#include <vector>
#include <string>

namespace agk {
namespace Widgets {

    struct LogMessage {
        spdlog::level::level_enum level;
        std::string time;
        std::string text;
    };

    class AppLogUI : public spdlog::sinks::base_sink<std::mutex> {
    public:
        AppLogUI() = default;
        
        void Render(const char* title);
        
        void Clear() {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_messages.clear();
        }

    protected:
        void sink_it_(const spdlog::details::log_msg& msg) override;
        void flush_() override {}

    private:
        std::vector<LogMessage> m_messages;
        std::mutex m_mutex;
        bool m_autoScroll = true;
    };

} // namespace Widgets
} // namespace agk
