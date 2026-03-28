#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace agk {

    /**
     * @brief Window settings for TrackerStudio.
     */
    struct WindowSettings {
        int width = 1280;
        int height = 720;
        bool vsync = true;
    };

    /**
     * @brief Preview settings for Live Monitoring.
     */
    struct PreviewSettings {
        int width = 640;
        int height = 360;
        int fps_limit = 30;
    };

    /**
     * @brief Main configuration for TrackerStudio.
     */
    struct StudioConfig {
        WindowSettings window;
        PreviewSettings preview;
        std::string last_session_path = "./recordings";

        bool Load(const std::string& path);
        bool Save(const std::string& path);
    };

} // namespace agk
