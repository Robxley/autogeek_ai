#include "AppLogUI.hpp"
#include "imgui.h"
#include <spdlog/spdlog.h>

namespace agk {
namespace Widgets {

    void AppLogUI::sink_it_(const spdlog::details::log_msg& msg) {
        spdlog::memory_buf_t formatted;
        spdlog::sinks::base_sink<std::mutex>::formatter_->format(msg, formatted);
        
        std::string full_text = fmt::to_string(formatted);

        std::lock_guard<std::mutex> lock(m_mutex);
        
        LogMessage logMsg;
        logMsg.level = msg.level;
        // Truncate timestamp part if needed, or just let format() do it
        logMsg.text = full_text;
        
        m_messages.push_back(std::move(logMsg));
        
        // Cap to 2000 lines to prevent infinite growth
        if (m_messages.size() > 2000) {
            m_messages.erase(m_messages.begin(), m_messages.begin() + 100);
        }
    }

    void AppLogUI::Render(const char* title) {
        if (!ImGui::Begin(title)) {
            ImGui::End();
            return;
        }

        // Options toolbar
        if (ImGui::Button("Clear")) Clear();
        ImGui::SameLine();
        ImGui::Checkbox("Auto-scroll", &m_autoScroll);

        ImGui::Separator();
        
        // Log display area
        ImGui::BeginChild("LogScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& msg : m_messages) {
            ImVec4 color;
            bool has_color = false;
            
            switch (msg.level) {
                case spdlog::level::info:     color = ImVec4(0.8f, 0.8f, 0.8f, 1.0f); has_color = true; break;
                case spdlog::level::warn:     color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f); has_color = true; break;
                case spdlog::level::err:      color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f); has_color = true; break;
                case spdlog::level::critical: color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); has_color = true; break;
                case spdlog::level::debug:    color = ImVec4(0.5f, 0.5f, 0.5f, 1.0f); has_color = true; break;
                default: break;
            }

            if (has_color) ImGui::PushStyleColor(ImGuiCol_Text, color);
            
            // TextUnformatted is faster than Text
            ImGui::TextUnformatted(msg.text.c_str());
            
            if (has_color) ImGui::PopStyleColor();
        }

        if (m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }

        ImGui::EndChild();
        ImGui::End();
    }

} // namespace Widgets
} // namespace agk
