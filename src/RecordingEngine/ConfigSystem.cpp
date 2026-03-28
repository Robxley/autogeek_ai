#include "ConfigSystem.hpp"
#include <fstream>
#include "Log.hpp"
import RecordingEngine;

using json = nlohmann::json;

namespace agk {

    // On utilise les macros de nlohmann_json pour la sérialisation si possible
    // ou on le fait manuellement pour avoir plus de contrôle sur les valeurs par défaut.

    void from_json(const json& j, VideoConfig& c) {
        c.target_fps = j.value("target_fps", c.target_fps);
        c.width = j.value("width", c.width);
        c.height = j.value("height", c.height);
        c.use_source_resolution = j.value("use_source_resolution", c.use_source_resolution);
        c.bitrate_kbps = j.value("bitrate_kbps", c.bitrate_kbps);
        c.encoder = j.value("encoder", c.encoder);
        c.format = j.value("format", c.format);
    }

    void from_json(const json& j, AudioConfig& c) {
        c.capture_system = j.value("capture_system", c.capture_system);
        c.capture_mic = j.value("capture_mic", c.capture_mic);
        c.system_device = j.value("system_device", c.system_device);
        c.mic_device = j.value("mic_device", c.mic_device);
        c.audio_bitrate_kbps = j.value("audio_bitrate_kbps", c.audio_bitrate_kbps);
    }

    void from_json(const json& j, InputsConfig& c) {
        c.enabled = j.value("enabled", c.enabled);
        c.capture_keyboard = j.value("capture_keyboard", c.capture_keyboard);
        c.capture_mouse = j.value("capture_mouse", c.capture_mouse);
        c.capture_gamepad = j.value("capture_gamepad", c.capture_gamepad);
        c.mouse_sampling_rate_ms = j.value("mouse_sampling_rate_ms", c.mouse_sampling_rate_ms);
        c.gamepad_polling_rate_ms = j.value("gamepad_polling_rate_ms", c.gamepad_polling_rate_ms);
        c.gamepad_deadzone = j.value("gamepad_deadzone", c.gamepad_deadzone);
        c.raw_input_mode = j.value("raw_input_mode", c.raw_input_mode);
    }

    bool ConfigSystem::LoadFromFile(const std::filesystem::path& path) {
        if (!std::filesystem::exists(path)) return false;
        try {
            std::ifstream file(path);
            json j;
            file >> j;
            return LoadFromString(j.dump());
        } catch (...) {
            return false;
        }
    }

    bool ConfigSystem::LoadFromString(const std::string& jsonStr) {
        try {
            json j = json::parse(jsonStr);
            
            if (j.contains("target")) {
                const auto& jt = j["target"];
                m_config.target.mode = jt.value("mode", m_config.target.mode);
                m_config.target.process_name = jt.value("process_name", m_config.target.process_name);
                m_config.target.window_title = jt.value("window_title", m_config.target.window_title);
                m_config.target.monitor_index = jt.value("monitor_index", m_config.target.monitor_index);
                m_config.target.include_cursor = jt.value("include_cursor", m_config.target.include_cursor);
                m_config.target.wait_for_target = jt.value("wait_for_target", m_config.target.wait_for_target);
                m_config.target.auto_pause_on_minimize = jt.value("auto_pause_on_minimize", m_config.target.auto_pause_on_minimize);
                m_config.target.auto_resume_on_restore = jt.value("auto_resume_on_restore", m_config.target.auto_resume_on_restore);
            }

            if (j.contains("recording")) {
                const auto& r = j["recording"];
                if (r.contains("video")) m_config.video = r["video"].get<VideoConfig>();
                if (r.contains("audio")) m_config.audio = r["audio"].get<AudioConfig>();
                if (r.contains("inputs")) m_config.inputs = r["inputs"].get<InputsConfig>();
            }

            // ... autres sections (storage, system) ...
            
            return true;
        } catch (const std::exception& e) {
            AGK_CORE_ERROR("[ConfigSystem] Error: {}", e.what());
            return false;
        }
    }

} // namespace agk
