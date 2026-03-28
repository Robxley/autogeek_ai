#include "SessionManager.hpp"
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <agk/RecordingEngine/Log.hpp>

namespace agk {

    SessionManager::SessionManager() {}

    bool SessionManager::CreateSession(const std::string& basePath, const Config& config) {
        // Step 1: Create a unique session name
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        
        std::stringstream ss;
        ss << std::put_time(std::localtime(&in_time_t), "session_%Y%m%d_%H%M%S");
        std::string sessionName = ss.str();

        m_sessionPath = std::filesystem::path(basePath) / sessionName;
        
        try {
            std::filesystem::create_directories(m_sessionPath);
            
            // Step 2: Write session_info.json
            nlohmann::json info;
            info["session_id"] = sessionName;
            info["start_time"] = std::chrono::system_clock::to_time_t(now);
            info["video"] = {
                {"width", config.video.width},
                {"height", config.video.height},
                {"fps", config.video.target_fps},
                {"encoder", config.video.encoder}
            };
            info["target"] = {
                {"process", config.target.process_name},
                {"window", config.target.window_title},
                {"mode", config.target.mode}
            };

            std::ofstream file(m_sessionPath / "session_info.json");
            if (file.is_open()) {
                file << info.dump(4);
                file.close();
            }

            AGK_CORE_INFO("[Session] Created new session at: {}", m_sessionPath.string());
            return true;
        } catch (const std::exception& e) {
            AGK_CORE_ERROR("[Session] Failed to create session directory: {}", e.what());
            return false;
        }
    }

} // namespace agk
