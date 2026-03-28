/**
 * @file main.cpp
 * @brief Console interface for the AutogeekAI Recording Engine.
 */

#include <agk/RecordingEngine/Log.hpp>

import RecordingEngine;
import <memory>;
import <thread>;
import <chrono>;

int main(int argc, char* argv[]) {
    agk::Log::Init();
    AGK_INFO("--- TrackerCLI Starting ---");

    std::string configPath = "";
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--config" && i + 1 < argc) {
            configPath = argv[++i];
        }
    }

    auto engine = agk::CreateEngine();
    if (engine) {
        if (engine->Initialize(configPath)) {
            engine->Start();
            
            AGK_INFO("[CLI] Recording for 3 seconds...");
            std::this_thread::sleep_for(std::chrono::seconds(3));
            
            engine->Stop();
        }
    }

    AGK_INFO("--- TrackerCLI Exiting ---");
    return 0;
}
