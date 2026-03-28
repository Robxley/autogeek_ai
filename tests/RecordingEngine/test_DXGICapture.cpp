#include <gtest/gtest.h>
#include "../../src/RecordingEngine/DXGICapture.hpp"

TEST(DXGICapture, Initialization) {
    agk::DXGICapture capture;
    // On Windows, monitor 0 should be available
    bool success = capture.Initialize(0);
    
    // Note: Cela peut échouer dans certain CI sans GPU/Ecran, mais en local ça doit passer
    if (!success) {
        GTEST_SKIP() << "DXGI Capture initialization failed (No monitor or GPU support?)";
    }

    int w, h;
    capture.GetOutputResolution(w, h);
    EXPECT_GT(w, 0);
    EXPECT_GT(h, 0);
}

TEST(DXGICapture, AcquireFrame) {
    agk::DXGICapture capture;
    if (!capture.Initialize(0)) {
        GTEST_SKIP() << "DXGI Capture initialization failed";
    }

    // On attend un peu pour être sûr qu'une frame est dispo
    auto frame = capture.AcquireNextFrame(500);
    if (frame) {
        EXPECT_NE(frame->data, nullptr);
        EXPECT_GT(frame->linesize, 0);
        capture.ReleaseFrame();
    } else {
        // Le timeout est possible si l'écran ne change pas (statique)
        std::cout << "[Test] No frame acquired (Screen might be static)" << std::endl;
    }
}
