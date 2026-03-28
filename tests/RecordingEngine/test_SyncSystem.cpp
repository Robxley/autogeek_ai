#include <gtest/gtest.h>
#include <thread>
#include <chrono>
import RecordingEngine;

using namespace std::chrono_literals;

TEST(SyncSystem, TimeProgression) {
    agk::SyncSystem sync;
    sync.Start();
    
    std::this_thread::sleep_for(50ms);
    auto t1 = sync.GetRelativeTimeMs();
    EXPECT_GE(t1, 45.0); // Allow for small scheduling jitters
    
    std::this_thread::sleep_for(50ms);
    auto t2 = sync.GetRelativeTimeMs();
    EXPECT_GT(t2, t1);
}

TEST(SyncSystem, PauseResume) {
    agk::SyncSystem sync;
    sync.Start();
    
    std::this_thread::sleep_for(50ms);
    sync.Pause();
    auto p1 = sync.GetRelativeTimeMs();
    
    // During pause, time should not progress
    std::this_thread::sleep_for(100ms);
    auto p2 = sync.GetRelativeTimeMs();
    EXPECT_NEAR(p1, p2, 0.1);
    
    sync.Resume();
    std::this_thread::sleep_for(50ms);
    auto r1 = sync.GetRelativeTimeMs();
    EXPECT_GT(r1, p1);
    // Total elapsed relative time should be ~100ms (50 before pause, 50 after)
    // On Windows, scheduling can introduce up to 30-50ms jitter.
    EXPECT_NEAR(r1, 100.0, 50.0); 
}
