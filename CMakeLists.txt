cmake_minimum_required(VERSION 3.28)

# Définir le projet
project(AutogeekAIRecordingEngine VERSION 1.0.0 LANGUAGES CXX)

# Activer C++20 et les modules
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Activer les modules C++20
set(CMAKE_EXPERIMENTAL_CXX_MODULE_CMAKE ON)

# Définir les options de build
option(BUILD_TESTING "Enable testing" ON)
option(BUILD_TRACKER_CLI "Build TrackerCLI" ON)
option(BUILD_TRACKER_STUDIO "Build TrackerStudio" ON)

# Définir les cibles
add_library(RecordingEngine STATIC)
add_executable(TrackerCLI)
add_executable(TrackerStudio)

# Configuration des dépendances
include(FetchContent)

# Intégration de spdlog
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.12.0
)
FetchContent_MakeAvailable(spdlog)

# Intégration de nlohmann_json
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# Intégration de FFmpeg (via vcpkg ou manuellement)
# TODO: Ajouter FFmpeg

# Intégration de SDL3
FetchContent_Declare(
    sdl3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG SDL3-3.0.0
)
FetchContent_MakeAvailable(sdl3)

# Intégration de Vulkan
find_package(Vulkan REQUIRED)

# Intégration de Dear ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.4
)
FetchContent_MakeAvailable(imgui)

# Intégration de ImGui SDL3 backend
FetchContent_Declare(
    imgui_sdl3
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.4
    SUBDIRECTORY examples/sdl3_opengl3
)
FetchContent_MakeAvailable(imgui_sdl3)

# Intégration de ImPlot
FetchContent_Declare(
    implot
    GIT_REPOSITORY https://github.com/epezent/implot.git
    GIT_TAG v0.15
)
FetchContent_MakeAvailable(implot)

# Configuration des cibles
target_link_libraries(RecordingEngine PRIVATE spdlog nlohmann_json)

target_link_libraries(TrackerCLI PRIVATE RecordingEngine)

target_link_libraries(TrackerStudio PRIVATE RecordingEngine imgui implot sdl3 Vulkan::Vulkan)

# Configuration des tests
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()