#include "RecordingConfigUI.hpp"
#include "imgui.h"
#include <string>
#include "portable-file-dialogs.h"
#include "../IconsFontAwesome6.h"
#include "UIHelpers.hpp"

namespace agk {
namespace Widgets {

    // Helper function for ImGui std::string inputs
    struct InputTextCallback_UserData {
        std::string* Str;
        ImGuiInputTextCallback ChainCallback;
        void* ChainCallbackUserData;
    };
    static int InputTextCallback(ImGuiInputTextCallbackData* data) {
        InputTextCallback_UserData* user_data = (InputTextCallback_UserData*)data->UserData;
        if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
            auto* str = user_data->Str;
            str->resize(data->BufTextLen);
            data->Buf = (char*)str->c_str();
        }
        if (user_data->ChainCallback) return user_data->ChainCallback(data);
        return 0;
    }
    static bool ImGuiInputTextStr(const char* label, std::string* str, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = nullptr, void* user_data = nullptr) {
        flags |= ImGuiInputTextFlags_CallbackResize;
        InputTextCallback_UserData cb_user_data;
        cb_user_data.Str = str;
        cb_user_data.ChainCallback = callback;
        cb_user_data.ChainCallbackUserData = user_data;
        return ImGui::InputText(label, (char*)str->c_str(), str->capacity() + 1, flags, InputTextCallback, &cb_user_data);
    }

    void RecordingConfigUI::Render(std::shared_ptr<IRecordingEngine> engine) {
        if (!engine) return;
        
        Config& cfg = engine->GetMutableConfig();

        if (ImGui::Button(ICON_FA_DOWNLOAD "##SaveCfg", ImVec2(28, 28))) {
            auto dest = pfd::save_file("Save Engine Config", "recording_config.json", {"JSON Files", "*.json", "All Files", "*"}).result();
            if (!dest.empty()) engine->SaveConfig(dest);
        }
        agk::UI::ItemTooltip("Save Configuration");
        
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_UPLOAD "##LoadCfg", ImVec2(28, 28))) {
            auto src = pfd::open_file("Load Engine Config", "", {"JSON Files", "*.json", "All Files", "*"}).result();
            if (!src.empty()) engine->LoadConfig(src[0]);
        }
        agk::UI::ItemTooltip("Load Configuration");
        
        bool isRec = engine->IsRecording();
        if (isRec) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "[Locked - Recording]");
        }
        
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        if (isRec) ImGui::BeginDisabled();
        bool needsRestart = false;

        if (ImGui::CollapsingHeader(ICON_FA_STAR " Main Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
            const char* modes[] = { "monitor_crop", "monitor", "window", "foreground" };
            int current_mode = 0;
            for (int i = 0; i < 4; ++i) {
                if (cfg.target.mode == modes[i]) current_mode = i;
            }
            if (ImGui::Combo("Capture Mode", &current_mode, modes, IM_ARRAYSIZE(modes))) {
                engine->SetCaptureMode(modes[current_mode]);
                needsRestart = true;
            }

            ImGuiInputTextStr("Process Filter", &cfg.target.process_name);
            ImGui::SameLine(); ImGui::TextDisabled("(for monitor_crop/window)");
            ImGuiInputTextStr("Window Title", &cfg.target.window_title);
            
            if (ImGui::Checkbox("Capture Audio", &cfg.audio.enabled)) needsRestart = true;
            ImGui::Checkbox("Include Cursor", &cfg.target.include_cursor);
            ImGui::Checkbox("Client Area Only (Ignore Title Bar)", &cfg.target.client_area_only);
            agk::UI::HelpMarker("When capturing a specific window, ignores the Windows title bar and borders if checked.");
        }

        if (ImGui::CollapsingHeader(ICON_FA_VIDEO " Video Configuration")) {
            ImGui::SliderInt("Target FPS", &cfg.video.target_fps, 10, 144);
            ImGui::SliderInt("Bitrate (Kbps)", &cfg.video.bitrate_kbps, 1000, 50000);
            
            ImGui::TextDisabled("Quick Resolution Scaling (based on current target):");
            if (ImGui::Button("1:1 (Native)")) {
                auto ts = engine->GetTargetState();
                if (ts.width > 0) { 
                    cfg.video.width = ts.width & ~1; 
                    cfg.video.height = ts.height & ~1; 
                    cfg.video.use_source_resolution = false; 
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("1:2 (Half)")) {
                auto ts = engine->GetTargetState();
                if (ts.width > 0) { 
                    cfg.video.width = (ts.width / 2) & ~1; 
                    cfg.video.height = (ts.height / 2) & ~1; 
                    cfg.video.use_source_resolution = false; 
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("1:4 (Quarter)")) {
                auto ts = engine->GetTargetState();
                if (ts.width > 0) { 
                    cfg.video.width = (ts.width / 4) & ~1; 
                    cfg.video.height = (ts.height / 4) & ~1; 
                    cfg.video.use_source_resolution = false; 
                }
            }

            ImGui::Checkbox("Use Source Resolution", &cfg.video.use_source_resolution);
            if (!cfg.video.use_source_resolution) {
                ImGui::InputInt("Target Width", &cfg.video.width);
                ImGui::InputInt("Target Height", &cfg.video.height);
            }
            ImGuiInputTextStr("Encoder", &cfg.video.encoder);
            ImGuiInputTextStr("Format", &cfg.video.format);
        }

        if (ImGui::CollapsingHeader(ICON_FA_MUSIC " Audio Configuration")) {
            if (!cfg.audio.enabled) ImGui::BeginDisabled();
            ImGui::Checkbox("Isolate Process Audio", &cfg.audio.is_process_isolated);
            ImGui::Checkbox("Capture Desktop Audio", &cfg.audio.capture_system);
            ImGui::Checkbox("Capture Microphone", &cfg.audio.capture_mic);
            ImGui::SliderInt("Audio Bitrate (Kbps)", &cfg.audio.audio_bitrate_kbps, 64, 320);
            if (!cfg.audio.enabled) ImGui::EndDisabled();
        }

        if (ImGui::CollapsingHeader(ICON_FA_GAMEPAD " Inputs Configuration")) {
            ImGui::Checkbox("Record Inputs", &cfg.inputs.enabled);
            if (!cfg.inputs.enabled) ImGui::BeginDisabled();
            ImGui::Checkbox("Capture Mouse", &cfg.inputs.capture_mouse);
            ImGui::Checkbox("Capture Keyboard", &cfg.inputs.capture_keyboard);
            ImGui::Checkbox("Capture Gamepad", &cfg.inputs.capture_gamepad);
            ImGui::Checkbox("Raw Input Mode", &cfg.inputs.raw_input_mode);
            agk::UI::HelpMarker("Captures inputs at the OS/driver level via RawInput API, preventing missed keystrokes during intensive gameplay. Required for low-latency telemetry.");
            ImGui::SliderInt("Mouse Polling (ms)", &cfg.inputs.mouse_sampling_rate_ms, 1, 50);
            ImGui::SliderInt("Gamepad Polling (ms)", &cfg.inputs.gamepad_polling_rate_ms, 1, 50);
            ImGui::SliderFloat("Gamepad Deadzone", &cfg.inputs.gamepad_deadzone, 0.0f, 1.0f);
            if (!cfg.inputs.enabled) ImGui::EndDisabled();
        }

        if (ImGui::CollapsingHeader(ICON_FA_MICROCHIP " Storage & System")) {
            ImGuiInputTextStr("Storage Root Path", &cfg.storage.base_output_path);
            ImGuiInputTextStr("Video Subfolder", &cfg.storage.video_subfolder);
            ImGui::Checkbox("Hardware Acceleration", &cfg.system.gpu_acceleration);
            agk::UI::HelpMarker("Utilize GPU decoding/encoding when available (e.g. NVIDIA NVENC). Strongly recommended for 60fps captures without system stutter.");
            ImGui::SliderInt("Internal FIFO Buffer", &cfg.system.internal_buffer_size, 10, 500);
            agk::UI::HelpMarker("Size of the multithreaded Queue. Increase if frames are dropping due to slow disk writes, but it will consume more RAM.");
            ImGui::Checkbox("Drop Frames on Buffer Full", &cfg.system.drop_frames_on_buffer_full);
        }

        if (isRec) ImGui::EndDisabled();

        if (needsRestart && engine->IsPreviewing()) {
            engine->Stop();
            engine->StartPreview();
        }
    }
}
}
