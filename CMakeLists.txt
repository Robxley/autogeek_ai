cmake_minimum_required(VERSION 3.28)

project(AutogeekAIRecordingEngine
    VERSION 1.0.0
    LANGUAGES CXX
)

# --- Configuration C++20 et Modules ---
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Activer explicitement les modules C++ (requis pour MSVC et CMake 3.28+)
if(MSVC)
    # CMake 3.28 gère automatiquement les flags pour les modules MSVC
endif()

# --- Options de Build ---
option(BUILD_TESTING "Activer les tests unitaires" ON)
option(BUILD_TRACKER_CLI "Compiler TrackerCLI" ON)
option(BUILD_TRACKER_STUDIO "Compiler TrackerStudio" ON)
option(BUILD_SNAKE_SIMULATOR "Compiler SnakeSimulator (Debug Target)" ON)

# --- Gestion des Dépendances ---
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(cmake/Dependencies.cmake)
include(cmake/FFmpegFetch.cmake)

# Vulkan (SDK requis)
find_package(Vulkan REQUIRED)

# --- Post-Build: Fonction de copie des DLLs (Windows) ---
function(agk_copy_dlls target)
    if(WIN32 AND TARGET ${target})
        # On récupère les DLLs de FFmpeg
        file(GLOB FF_DLLS "${FFMPEG_BIN_DIR}/*.dll")
        
        # Commande de copie
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL3::SDL3> $<TARGET_FILE_DIR:${target}>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${FF_DLLS} $<TARGET_FILE_DIR:${target}>
            COMMENT "Copying runtime DLLs for ${target}"
        )
    endif()
endfunction()

# --- Cibles Principales ---

# Dossier Source (Module Core)
add_subdirectory(src/RecordingEngine)

# Applications CLI & Studio
if(BUILD_TRACKER_CLI)
    add_subdirectory(src/TrackerCLI)
endif()

if(BUILD_TRACKER_STUDIO)
    add_subdirectory(src/TrackerStudio)
endif()

if(BUILD_SNAKE_SIMULATOR)
    add_subdirectory(src/SnakeSimulator)
endif()

# --- Tests ---
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
