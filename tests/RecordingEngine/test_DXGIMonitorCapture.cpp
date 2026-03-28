#include <gtest/gtest.h>
#include "../../src/RecordingEngine/DXGIMonitorCapture.hpp"
#include <thread>
#include <chrono>

using namespace agk;

class DXGIMonitorCaptureTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Prepare Capture
    }

    void TearDown() override {
        // Cleanup if necessary
    }
};

TEST_F(DXGIMonitorCaptureTest, InitializationAndFrameAcquisition) {
    auto capture = std::make_unique<DXGIMonitorCapture>();

    bool initResult = capture->Initialize(0); // Monitor 0
    if (!initResult) {
        GTEST_SKIP() << "Skipping test: DXGIMonitorCapture failed to initialize (common over RDP or hybrid GPUs).";
    }
    
    EXPECT_TRUE(capture->IsInitialized());

    // Acquire Frame with a generous timeout to ensure desktop rendering gets flushed
    std::unique_ptr<CaptureFrame> frame = nullptr;
    int retries = 5;
    while (!frame && retries > 0) {
        frame = capture->AcquireNextFrame(500); // 500ms timeout
        if (!frame) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        retries--;
    }

    ASSERT_NE(frame, nullptr) << "Failed to acquire a single frame from the Desktop Duplication API within bounds.";
    
    EXPECT_GT(frame->width, 0);
    EXPECT_GT(frame->height, 0);
    EXPECT_GT(frame->linesize, 0);
    EXPECT_NE(frame->data, nullptr);

    // Print out the fetched frame dimensions for debug
    std::cout << "[Test] Successfully acquired a DXGI frame: " 
              << frame->width << "x" << frame->height 
              << " | Linesize: " << frame->linesize << " bytes" << std::endl;
}

TEST_F(DXGIMonitorCaptureTest, StopAndCleanup) {
    auto capture = std::make_unique<DXGIMonitorCapture>();
    if (!capture->Initialize(0)) {
        GTEST_SKIP() << "Skipping test due to environment limitations.";
    }
    capture->Stop();
    EXPECT_FALSE(capture->IsInitialized());
}
