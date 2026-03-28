#include <gtest/gtest.h>
#include "../../src/RecordingEngine/EncoderModule.hpp"
#include "../../src/RecordingEngine/Log.hpp"
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

TEST(EncoderModule, BasicRecording) {
    agk::Log::Init();
    agk::EncoderModule encoder;
    agk::VideoConfig vConfig;
    agk::AudioConfig aConfig;
    aConfig.enabled = false; // Disable audio for this basic test

    vConfig.width = 1280;
    vConfig.height = 720;
    vConfig.target_fps = 60;
    vConfig.bitrate_kbps = 4000;

    std::string outputFile = "test_output.mkv";
    if (fs::exists(outputFile)) fs::remove(outputFile);

    ASSERT_TRUE(encoder.Initialize(outputFile, vConfig, aConfig, nullptr));

    // Generate 60 frames (1 second at 60fps)
    size_t bufferSize = vConfig.width * vConfig.height * 4;
    std::vector<uint8_t> dummyFrame(bufferSize, 0);
    
    // Fill with blue color
    for (size_t i = 0; i < bufferSize; i += 4) {
        dummyFrame[i] = 255;     // B
        dummyFrame[i + 1] = 0;   // G
        dummyFrame[i + 2] = 0;   // R
        dummyFrame[i + 3] = 255; // A
    }

    for (int i = 0; i < 60; ++i) {
        ASSERT_TRUE(encoder.EncodeVideoFrame(dummyFrame.data(), vConfig.width * 4, i));
    }

    encoder.Finalize();

    ASSERT_TRUE(fs::exists(outputFile));
    ASSERT_GT(fs::file_size(outputFile), 0);
    
    // Cleanup
    // fs::remove(outputFile);
}
