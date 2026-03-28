#pragma once

#include <string>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <agk/RecordingEngine/ConfigSystem.hpp>

namespace agk {

    /**
     * @brief Manages the recording session lifecycle and metadata.
     */
    class SessionManager {
    public:
        SessionManager();
        ~SessionManager() = default;

        /**
         * @brief Creates the session directory and the session_info.json file.
         */
        bool CreateSession(const std::string& basePath, const Config& config);

        /**
         * @brief Returns the absolute path of the current session.
         */
        std::string GetSessionPath() const { return m_sessionPath.string(); }

    private:
        std::filesystem::path m_sessionPath;
    };

} // namespace agk
