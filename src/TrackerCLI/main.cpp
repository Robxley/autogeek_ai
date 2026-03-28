// main.cpp for TrackerCLI
#include <iostream>
#include <string>

// Import the RecordingEngine module
import RecordingEngine;

int main(int argc, char* argv[]) {
    std::cout << "TrackerCLI - Starting..." << std::endl;
    
    // Test the RecordingEngine module
    TestRecordingEngine();
    
    std::cout << "TrackerCLI - Exiting..." << std::endl;
    return 0;
}