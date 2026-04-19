#include <gtest/gtest.h>
#include <agk/RecordingEngine/IRecordingEngine.hpp>
#include "../../src/RecordingEngine/EventSerializer.hpp"
#include "../../src/RecordingEngine/RawInputModule.hpp"
#include <filesystem>
#include <vector>
#include <fstream>
#include <nlohmann/json.hpp>
#include <iostream>

using namespace agk;
using json = nlohmann::json;

class InputSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        testFile = "test_events.jsonl";
        if (std::filesystem::exists(testFile)) {
            std::filesystem::remove(testFile);
        }
    }

    void TearDown() override {
        // Keep files for manual inspection if needed, or remove
        // if (std::filesystem::exists(testFile)) std::filesystem::remove(testFile);
    }

    std::string testFile;
};

TEST_F(InputSystemTest, SerializerBundlingAndPrecision) {
    {
        EventSerializer serializer;
        ASSERT_TRUE(serializer.Open(testFile));

        InputEvent ev1;
        ev1.frameIndex = 1;
        ev1.timestamp = 10.12345;
        ev1.type = InputType::MouseMove;
        ev1.x = 0.5009;
        ev1.y = 0.1234;
        ev1.dx = 0.001;
        ev1.dy = 0.0001;

        serializer.PushEvent(ev1);

        InputEvent ev2;
        ev2.frameIndex = 1;
        ev2.timestamp = 10.8888; 
        ev2.type = InputType::KeyDown;
        ev2.vkCode = 65; // 'A'

        serializer.PushEvent(ev2);

        // Trigger flush by pushing next frame
        InputEvent ev3;
        ev3.frameIndex = 2;
        ev3.timestamp = 40.0;
        ev3.type = InputType::MouseMove;
        serializer.PushEvent(ev3);

        serializer.Close();
    } 

    std::ifstream file(testFile);
    std::string line;
    ASSERT_TRUE(std::getline(file, line));
    
    json j = json::parse(line);
    EXPECT_EQ(j["frame_index"], 1);
    ASSERT_TRUE(j.contains("timestamps"));
    ASSERT_EQ(j["timestamps"].size(), 2);

    // First group (10ms)
    EXPECT_EQ(j["timestamps"][0]["time"], 10);
    EXPECT_TRUE(j["timestamps"][0].contains("mouse"));
    EXPECT_FALSE(j["timestamps"][0].contains("keyboard"));
    EXPECT_NEAR(j["timestamps"][0]["mouse"][0]["x"].get<double>(), 0.501, 0.0001);

    // Second group (11ms)
    EXPECT_EQ(j["timestamps"][1]["time"], 11);
    EXPECT_TRUE(j["timestamps"][1].contains("keyboard"));
    EXPECT_EQ(j["timestamps"][1]["keyboard"][0]["vk"], 65);
}

TEST_F(InputSystemTest, MouseDeduplicationInEngine) {
    {
        EventSerializer serializer;
        serializer.Open(testFile);
        
        InputEvent ev;
        ev.frameIndex = 10;
        ev.timestamp = 100.0;
        ev.type = InputType::MouseMove;
        ev.x = 0.1; ev.y = 0.2;
        
        serializer.PushEvent(ev);
    } 

    std::ifstream file(testFile);
    std::string line;
    ASSERT_TRUE(std::getline(file, line));
    json j = json::parse(line);
    
    EXPECT_EQ(j["frame_index"], 10);
    EXPECT_FALSE(j["timestamps"][0].contains("keyboard"));
}
