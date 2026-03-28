# CMakeLists.txt for tests

# Enable testing
enable_testing()

# Add GoogleTest
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.12.1
)
FetchContent_MakeAvailable(googletest)

# Add a simple test executable
add_executable(TestRecordingEngine
    ${CMAKE_CURRENT_SOURCE_DIR}/TestRecordingEngine.cpp
)

# Include directories
target_include_directories(TestRecordingEngine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/RecordingEngine/include
)

# Link libraries
target_link_libraries(TestRecordingEngine PRIVATE RecordingEngine gtest gtest_main)

# Add the test to CTest
add_test(NAME TestRecordingEngine COMMAND TestRecordingEngine)