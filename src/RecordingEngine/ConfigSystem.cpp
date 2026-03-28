#include <agk/RecordingEngine/ConfigSystem.hpp>
#include <fstream>
#include <agk/RecordingEngine/Log.hpp>
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

    void ConfigSystem::UpdateTargetProcess(const std::string& processName) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_config.target.process_name = processName;
    }

    void ConfigSystem::UpdateTargetWindow(const std::string& windowTitle) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_config.target.window_title = windowTitle;
    }

    void ConfigSystem::UpdateTargetMode(const std::string& mode) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_config.target.mode = mode;
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

            if (j.contains("storage")) {
                const auto& s = j["storage"];
                m_config.storage.base_output_path = s.value("base_output_path", m_config.storage.base_output_path);
                m_config.storage.video_subfolder = s.value("video_subfolder", m_config.storage.video_subfolder);
                m_config.storage.events_filename = s.value("events_filename", m_config.storage.events_filename);
            }

            if (j.contains("system")) {
                const auto& s = j["system"];
                m_config.system.thread_priority = s.value("thread_priority", m_config.system.thread_priority);
                m_config.system.gpu_acceleration = s.value("gpu_acceleration", m_config.system.gpu_acceleration);
                m_config.system.internal_buffer_size = s.value("internal_buffer_size", m_config.system.internal_buffer_size);
                m_config.system.drop_frames_on_buffer_full = s.value("drop_frames_on_buffer_full", m_config.system.drop_frames_on_buffer_full);
                m_config.system.log_directory = s.value("log_directory", m_config.system.log_directory);
            }
            
            return true;
        } catch (const std::exception& e) {
            AGK_CORE_ERROR("[ConfigSystem] Error: {}", e.what());
            return false;
        }
    }

    std::string ConfigSystem::SaveToString() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        json j;
        
        j["target"]["mode"] = m_config.target.mode;
        j["target"]["process_name"] = m_config.target.process_name;
        j["target"]["window_title"] = m_config.target.window_title;
        j["target"]["monitor_index"] = m_config.target.monitor_index;
        j["target"]["include_cursor"] = m_config.target.include_cursor;
        j["target"]["wait_for_target"] = m_config.target.wait_for_target;
        j["target"]["auto_pause_on_minimize"] = m_config.target.auto_pause_on_minimize;
        j["target"]["auto_resume_on_restore"] = m_config.target.auto_resume_on_restore;

        json& rv = j["recording"]["video"];
        rv["target_fps"] = m_config.video.target_fps;
        rv["width"] = m_config.video.width;
        rv["height"] = m_config.video.height;
        rv["use_source_resolution"] = m_config.video.use_source_resolution;
        rv["bitrate_kbps"] = m_config.video.bitrate_kbps;
        rv["encoder"] = m_config.video.encoder;
        rv["format"] = m_config.video.format;

        json& ra = j["recording"]["audio"];
        ra["enabled"] = m_config.audio.enabled;
        ra["is_process_isolated"] = m_config.audio.is_process_isolated;
        ra["capture_system"] = m_config.audio.capture_system;
        ra["capture_mic"] = m_config.audio.capture_mic;
        ra["system_device"] = m_config.audio.system_device;
        ra["mic_device"] = m_config.audio.mic_device;
        ra["audio_bitrate_kbps"] = m_config.audio.audio_bitrate_kbps;

        json& ri = j["recording"]["inputs"];
        ri["enabled"] = m_config.inputs.enabled;
        ri["capture_keyboard"] = m_config.inputs.capture_keyboard;
        ri["capture_mouse"] = m_config.inputs.capture_mouse;
        ri["capture_gamepad"] = m_config.inputs.capture_gamepad;
        ri["mouse_sampling_rate_ms"] = m_config.inputs.mouse_sampling_rate_ms;
        ri["gamepad_polling_rate_ms"] = m_config.inputs.gamepad_polling_rate_ms;
        ri["gamepad_deadzone"] = m_config.inputs.gamepad_deadzone;
        ri["raw_input_mode"] = m_config.inputs.raw_input_mode;

        json& s = j["storage"];
        s["base_output_path"] = m_config.storage.base_output_path;
        s["video_subfolder"] = m_config.storage.video_subfolder;
        s["events_filename"] = m_config.storage.events_filename;

        json& sys = j["system"];
        sys["thread_priority"] = m_config.system.thread_priority;
        sys["gpu_acceleration"] = m_config.system.gpu_acceleration;
        sys["internal_buffer_size"] = m_config.system.internal_buffer_size;
        sys["drop_frames_on_buffer_full"] = m_config.system.drop_frames_on_buffer_full;
        sys["log_directory"] = m_config.system.log_directory;

        return j.dump(4);
    }

    bool ConfigSystem::SaveToFile(const std::filesystem::path& path) const {
        try {
            std::string content = SaveToString();
            std::ofstream file(path);
            if (!file) return false;
            file << content;
            return true;
        } catch (...) {
            return false;
        }
    }

} // namespace agk
