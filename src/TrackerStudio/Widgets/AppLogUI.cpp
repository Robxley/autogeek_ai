#include "AppLogUI.hpp"
#include "imgui.h"
#include <spdlog/spdlog.h>
#include "../IconsFontAwesome6.h"

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
        
        // Log display area in a table for perfect alignment
        if (ImGui::BeginTable("LogTable", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("Lvl", ImGuiTableColumnFlags_WidthFixed, 30.0f);
            ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            std::lock_guard<std::mutex> lock(m_mutex);
            for (const auto& msg : m_messages) {
                ImGui::TableNextRow();
                
                ImVec4 color = ImVec4(1, 1, 1, 1);
                const char* icon = ICON_FA_CIRCLE_INFO;

                switch (msg.level) {
                    case spdlog::level::info:     color = ImVec4(0.4f, 0.8f, 1.0f, 1.0f); icon = ICON_FA_CIRCLE_INFO; break;
                    case spdlog::level::warn:     color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f); icon = ICON_FA_CIRCLE_EXCLAMATION; break;
                    case spdlog::level::err:      color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f); icon = ICON_FA_CIRCLE_XMARK; break;
                    case spdlog::level::critical: color = ImVec4(1.0f, 0.1f, 0.1f, 1.0f); icon = ICON_FA_CIRCLE_RADIATION; break;
                    case spdlog::level::debug:    color = ImVec4(0.6f, 0.6f, 0.6f, 1.0f); icon = ICON_FA_BUG; break;
                    default: break;
                }

                // Column: Level Icon
                ImGui::TableSetColumnIndex(0);
                ImGui::TextColored(color, "%s", icon);
                
                // Column: Time (extracting from spdlog default format [HH:MM:SS])
                // Note: full_text usually starts with "[Timestamp] "
                ImGui::TableSetColumnIndex(1);
                size_t bracketOpen = msg.text.find('[');
                size_t bracketClose = msg.text.find(']');
                if (bracketOpen != std::string::npos && bracketClose != std::string::npos) {
                    std::string ts = msg.text.substr(bracketOpen + 1, bracketClose - bracketOpen - 1);
                    ImGui::TextDisabled("%s", ts.c_str());
                } else {
                    ImGui::TextDisabled("--:--:--");
                }

                // Column: Message
                ImGui::TableSetColumnIndex(2);
                size_t msgStart = (bracketClose != std::string::npos) ? bracketClose + 1 : 0;
                ImGui::TextUnformatted(msg.text.c_str() + msgStart);
            }

            if (m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                ImGui::SetScrollHereY(1.0f);
            }
            ImGui::EndTable();
        }

        ImGui::End();
    }

} // namespace Widgets
} // namespace agk
