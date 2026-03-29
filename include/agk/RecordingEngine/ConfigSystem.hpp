#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <mutex>

namespace agk {

    struct VideoConfig {
        int target_fps = 60;
        int width = 1920;
        int height = 1080;
        bool use_source_resolution = true;
        int bitrate_kbps = 8000;
        std::string encoder = "h264_nvenc";
        std::string format = "mkv";
    };

    struct AudioConfig {
        bool enabled = false;
        bool is_process_isolated = true;
        bool capture_system = false;
        bool capture_mic = false;
        std::string system_device = "default";
        std::string mic_device = "default";
        int audio_bitrate_kbps = 192;
    };

    struct InputsConfig {
        bool enabled = true;
        bool capture_keyboard = true;
        bool capture_mouse = true;
        bool capture_gamepad = true;
        int mouse_sampling_rate_ms = 10;
        int gamepad_polling_rate_ms = 8;
        float gamepad_deadzone = 0.1f;
        bool raw_input_mode = true;
    };

    struct Config {
        struct Target {
            std::string mode = "monitor_crop";
            std::string process_name = "";
            std::string window_title = "";
            int monitor_index = 0;
            bool include_cursor = true;
            bool wait_for_target = true;
            bool auto_pause_on_minimize = true;
            bool auto_resume_on_restore = true;
        } target;

        struct Hotkeys {
            bool enabled = true;
            std::string start_stop = "Ctrl+Shift+R";
            std::string pause_resume = "Ctrl+Shift+P";
            std::string add_marker = "Ctrl+Shift+M";
        } hotkeys;

        VideoConfig video;
        AudioConfig audio;
        InputsConfig inputs;

        struct Storage {
            std::string base_output_path = "./recordings";
            std::string video_subfolder = "video";
            std::string events_filename = "events.jsonl";
        } storage;

        struct System {
            std::string thread_priority = "high";
            bool gpu_acceleration = true;
            int internal_buffer_size = 60;
            bool drop_frames_on_buffer_full = true;
            std::string log_directory = "logs";
        } system;
    };

    /**
     * @brief System responsible for loading and managing the engine configuration.
     */
    class ConfigSystem {
    public:
        ConfigSystem() = default;

        bool LoadFromFile(const std::filesystem::path& path);
        bool LoadFromString(const std::string& jsonStr);

        void UpdatePreviewConfig(int width, int height);
        void UpdateTargetWindow(const std::string& windowTitle);
        void UpdateTargetProcess(const std::string& processName);
        void UpdateTargetMode(const std::string& mode);

        bool SaveToFile(const std::filesystem::path& path) const;
        std::string SaveToString() const;

        const Config& GetConfig() const { return m_config; }
        Config& GetMutableConfig() { return m_config; }

    private:
        Config m_config;
        mutable std::mutex m_mutex;
    };

} // namespace agk
