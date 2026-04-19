#include <gtest/gtest.h>
#include "../../src/RecordingEngine/TargetTracker.hpp"
#include "../../include/agk/RecordingEngine/Log.hpp"

TEST(TargetTracker, WindowSearchOptimized) {
    agk::Log::Init();
    agk::TargetTracker tracker;
    
    // Test with a known window title
    std::string windowTitle = "Calculator";
    bool result = tracker.Update("", windowTitle);
    
    if (result) {
        auto info = tracker.GetInfo();
        ASSERT_NE(info.hwnd, nullptr);
        ASSERT_GT(info.width, 0);
        ASSERT_GT(info.height, 0);
    } else {
        AGK_CORE_WARN("[TargetTracker] Calculator window not found for test");
    }
}

TEST(TargetTracker, ProcessSearchOptimized) {
    agk::Log::Init();
    agk::TargetTracker tracker;
    
    // Test with a known process name
    std::string processName = "calc";
    bool result = tracker.Update(processName, "");
    
    if (result) {
        auto info = tracker.GetInfo();
        ASSERT_NE(info.hwnd, nullptr);
        ASSERT_GT(info.width, 0);
        ASSERT_GT(info.height, 0);
    } else {
        AGK_CORE_WARN("[TargetTracker] Calculator process not found for test");
    }
}

TEST(TargetTracker, NormalizeCoordinates) {
    agk::Log::Init();
    agk::TargetTracker tracker;
    
    // Test coordinate normalization
    double outX, outY;
    tracker.NormalizeCoordinates(100, 100, outX, outY);
    
    ASSERT_GE(outX, 0.0);
    ASSERT_LE(outX, 1.0);
    ASSERT_GE(outY, 0.0);
    ASSERT_LE(outY, 1.0);
}