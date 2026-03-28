// TestRecordingEngine.cpp
#include <gtest/gtest.h>

// Import the RecordingEngine module
import RecordingEngine;

// Simple test to verify the module loads correctly
TEST(RecordingEngineTest, ModuleLoads) {
    // This test will pass if the module loads without errors
    ASSERT_NO_THROW(TestRecordingEngine());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}