#include "StudioConfig.hpp"
#include <fstream>
#include <iostream>

namespace agk {

    bool StudioConfig::Load(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) return false;

        try {
            nlohmann::json j;
            file >> j;

            if (j.contains("window")) {
                window.width = j["window"].value("width", 1280);
                window.height = j["window"].value("height", 720);
                window.vsync = j["window"].value("vsync", true);
            }

            if (j.contains("preview")) {
                preview.width = j["preview"].value("width", 640);
                preview.height = j["preview"].value("height", 360);
                preview.fps_limit = j["preview"].value("fps_limit", 30);
            }

            last_session_path = j.value("last_session_path", "recordings");

            return true;
        } catch (const std::exception& e) {
            std::cerr << "[StudioConfig] Error loading: " << e.what() << std::endl;
            return false;
        }
    }

    bool StudioConfig::Save(const std::string& path) {
        nlohmann::json j;
        j["window"]["width"] = window.width;
        j["window"]["height"] = window.height;
        j["window"]["vsync"] = window.vsync;

        j["preview"]["width"] = preview.width;
        j["preview"]["height"] = preview.height;
        j["preview"]["fps_limit"] = preview.fps_limit;

        j["last_session_path"] = last_session_path;

        try {
            std::ofstream file(path);
            file << j.dump(4);
            return true;
        } catch (const std::exception& e) {
            std::cerr << "[StudioConfig] Error saving: " << e.what() << std::endl;
            return false;
        }
    }

} // namespace agk
