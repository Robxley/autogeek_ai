/**
 * @file main.cpp
 * @brief Console interface for the AutogeekAI Recording Engine.
 */

import RecordingEngine;
import <iostream>;
import <memory>;
import <thread>;
import <chrono>;

int main() {
    std::cout << "--- TrackerCLI Starting ---" << std::endl;

    auto engine = agk::CreateEngine();
    if (engine) {
        if (engine->Initialize()) {
            engine->Start();
            
            std::cout << "[CLI] Recording for 3 seconds..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(3));
            
            engine->Stop();
        }
    }

    std::cout << "--- TrackerCLI Exiting ---" << std::endl;
    return 0;
}
