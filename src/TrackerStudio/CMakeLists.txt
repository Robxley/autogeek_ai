# CMakeLists.txt for TrackerStudio

# Add the TrackerStudio executable
add_executable(TrackerStudio
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

# Include directories
target_include_directories(TrackerStudio PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../RecordingEngine/include
)

# Link libraries
target_link_libraries(TrackerStudio PRIVATE RecordingEngine imgui implot sdl3 Vulkan::Vulkan)