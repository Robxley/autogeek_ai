# CMakeLists.txt for RecordingEngine

# Add the RecordingEngine library
add_library(RecordingEngine STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/RecordingEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/RecordingEngine/RecordingEngine.ixx
)

# Include directories
target_include_directories(RecordingEngine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# Link libraries
target_link_libraries(RecordingEngine PRIVATE spdlog nlohmann_json)

# Enable C++20 modules
set_target_properties(RecordingEngine PROPERTIES CXX_MODULES ON)