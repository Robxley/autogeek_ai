// main.cpp for TrackerStudio
#include <iostream>
#include <string>

// Import the RecordingEngine module
import RecordingEngine;

int main(int argc, char* argv[]) {
    std::cout << "TrackerStudio - Starting..." << std::endl;
    
    // Test the RecordingEngine module
    TestRecordingEngine();
    
    std::cout << "TrackerStudio - Exiting..." << std::endl;
    return 0;
}