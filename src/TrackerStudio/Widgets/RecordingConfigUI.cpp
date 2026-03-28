#include "RecordingConfigUI.hpp"
#include "imgui.h"
#include <string>
#include "portable-file-dialogs.h"
#include "../IconsFontAwesome6.h"

namespace agk {
namespace Widgets {

    void RecordingConfigUI::Render(std::shared_ptr<IRecordingEngine> engine) {
        if (!engine) return;

        // Retrieve config and cast away constness locally for ImGui binding.
        // It's safe here because IRecordingEngine provides SaveConfig which will read the modified struct.
        // (If we had a pure setter for everything it would be too bloated, so we usually expose a mutable config or sync it).
        // Since GetConfig() is const, wait! Let's just use ImGui and apply settings.
        
        static char processBuffer[128] = "";
        static bool firstRun = true;
        
        const Config& cfg = engine->GetConfig();
        
        if (firstRun) {
            strncpy(processBuffer, cfg.target.process_name.c_str(), sizeof(processBuffer) - 1);
            firstRun = false;
        }

        ImGui::Text(ICON_FA_WRENCH " Recording Settings");
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::Text(ICON_FA_CROSSHAIRS " Target Selection");
        
        const char* modes[] = { "monitor_crop", "monitor", "window" };
        int current_mode = 0;
        for (int i = 0; i < 3; ++i) {
            if (cfg.target.mode == modes[i]) current_mode = i;
        }

        if (ImGui::Combo("Capture Mode", &current_mode, modes, IM_ARRAYSIZE(modes))) {
            engine->SetCaptureMode(modes[current_mode]);
        }

        if (ImGui::InputText("Target Process", processBuffer, IM_ARRAYSIZE(processBuffer))) {
            engine->SetTargetProcess(processBuffer);
        }

        ImGui::Spacing(); ImGui::Spacing();
        ImGui::Text(ICON_FA_FLOPPY_DISK " Configuration Management");
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button(ICON_FA_DOWNLOAD " Save Config", ImVec2(120, 0))) {
            auto dest = pfd::save_file("Save Engine Config", "recording_config.json", {"JSON Files", "*.json", "All Files", "*"}).result();
            if (!dest.empty()) {
                engine->SaveConfig(dest);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_UPLOAD " Load Config", ImVec2(120, 0))) {
            auto src = pfd::open_file("Load Engine Config", "", {"JSON Files", "*.json", "All Files", "*"}).result();
            if (!src.empty()) {
                engine->LoadConfig(src[0]);
                strncpy(processBuffer, engine->GetConfig().target.process_name.c_str(), sizeof(processBuffer) - 1);
            }
        }
    }

}
}
