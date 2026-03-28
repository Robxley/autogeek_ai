# CMakeLists.txt for TrackerCLI

# Add the TrackerCLI executable
add_executable(TrackerCLI
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

# Include directories
target_include_directories(TrackerCLI PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../RecordingEngine/include
)

# Link libraries
target_link_libraries(TrackerCLI PRIVATE RecordingEngine)