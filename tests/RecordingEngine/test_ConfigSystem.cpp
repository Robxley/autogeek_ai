#include <gtest/gtest.h>
import RecordingEngine;

// On teste l'existence du ConfigSystem (sera implémenté dans le module)
// Pour l'instant on va simuler l'usage via une classe interne au module si possible
// ou une classe exportée.

namespace agk {
    // Forward declaration of what we want to test
    // Idéalement, ConfigSystem sera une classe interne gérée par l'engine
    // mais pour le test unitaire on veut pouvoir y accéder.
}

TEST(ConfigSystem, DefaultValues) {
    agk::ConfigSystem config;
    const auto& c = config.GetConfig();
    EXPECT_EQ(c.video.format, "mkv");
    EXPECT_EQ(c.video.target_fps, 60);
    EXPECT_TRUE(c.video.use_source_resolution);
}

TEST(ConfigSystem, LoadJsonString) {
    std::string jsonStr = R"({"recording": {"video": {"target_fps": 120, "format": "mp4"}}})";
    agk::ConfigSystem config;
    ASSERT_TRUE(config.LoadFromString(jsonStr));
    
    const auto& c = config.GetConfig();
    EXPECT_EQ(c.video.target_fps, 120);
    EXPECT_EQ(c.video.format, "mp4");
}

