#include "SessionExplorer.hpp"
#include "imgui.h"
#include <filesystem>
#include "portable-file-dialogs.h"
#include "../IconsFontAwesome6.h"
#include <agk/RecordingEngine/Log.hpp>
#include <algorithm>

namespace agk {
namespace Widgets {

    void SessionExplorer::Render(StudioConfig& config, std::shared_ptr<IRecordingEngine> engine, SessionReplayer* replayer) {
        if (ImGui::Button(ICON_FA_FOLDER_OPEN " Set Root Folder")) {
            std::string new_path = pfd::select_folder("Choose Recordings Folder", config.last_session_path).result();
            if (!new_path.empty()) {
                config.last_session_path = new_path;
                config.Save("tracker_config.json");
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_LINK " Open In Explorer") && !config.last_session_path.empty()) {
            std::string cmd = "explorer \"" + config.last_session_path + "\"";
            system(cmd.c_str());
        }

        ImGui::Separator();
        ImGui::TextDisabled("Root: %s", config.last_session_path.empty() ? "None" : config.last_session_path.c_str());
        ImGui::Separator();

        if (config.last_session_path.empty() || !std::filesystem::exists(config.last_session_path)) {
            ImGui::TextWrapped("Please set a valid root folder to view sessions.");
            return;
        }

        ImGui::BeginChild("SessionList", ImVec2(0, 0), true);

        try {
            // Sort paths in reverse chronological order
            std::vector<std::filesystem::path> paths;
            for (const auto& entry : std::filesystem::directory_iterator(config.last_session_path)) {
                if (entry.is_directory()) {
                    paths.push_back(entry.path());
                }
            }
            
            std::sort(paths.begin(), paths.end(), std::greater<std::filesystem::path>());

            for (const auto& path : paths) {
                std::string folderName = path.filename().string();
                
                // Use a folder icon
                bool isSelected = (replayer->GetCurrentSessionPath() == path.string());
                
                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
                if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;

                ImGui::TreeNodeEx((void*)path.c_str(), flags, "%s %s", ICON_FA_FOLDER, folderName.c_str());
                if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                    // Update replayer on click
                    replayer->OpenSession(path.string());
                    AGK_INFO("Loaded session: {}", folderName);
                }
            }
        } catch (const std::exception& e) {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error reading directory: %s", e.what());
        }

        ImGui::EndChild();
    }

}
}
