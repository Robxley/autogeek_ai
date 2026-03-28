#include "RecordingConfigUI.hpp"
#include "imgui.h"
#include <string>

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

        ImGui::Begin("Recording Configuration");

        ImGui::Text("Target Selection");
        ImGui::Separator();
        
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

        ImGui::Spacing();
        ImGui::Text("Configuration Management");
        ImGui::Separator();

        if (ImGui::Button("Save Config")) {
            engine->SaveConfig("recording_config.json");
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Config")) {
            engine->LoadConfig("recording_config.json");
            // Refresh buffer
            strncpy(processBuffer, engine->GetConfig().target.process_name.c_str(), sizeof(processBuffer) - 1);
        }

        ImGui::End();
    }

}
}
