# src/RecordingEngine/CMakeLists.txt

# Configurer la bibliothèque de modules
add_library(RecordingEngine STATIC)

# Ajouter les fichiers sources du module
target_sources(RecordingEngine
    PUBLIC
        FILE_SET CXX_MODULES FILES
            RecordingEngine.ixx
    PRIVATE
        RecordingEngineImpl.cpp
        ConfigSystem.cpp
        ImageUtils.cpp
        Log.cpp
        DXGIMonitorCapture.cpp
        WGCWindowCapture.cpp
        EncoderModule.cpp
        RawInputModule.cpp
        EventSerializer.cpp
        TargetTracker.cpp
        SessionManager.cpp
        WASAPICapture.cpp
        GamepadModule.cpp
)

# Liens avec les bibliothèques système Windows pour DXGI et WinRT
if(WIN32)
    target_link_libraries(RecordingEngine PRIVATE d3d11 dxgi ole32 windowsapp dwmapi xinput)
endif()


# Configurer les dossiers d'include
target_include_directories(RecordingEngine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Lier les dépendances (spdlog, nlohmann_json, Vulkan si besoin plus tard)
target_link_libraries(RecordingEngine
    PUBLIC
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        FFmpeg::avformat
        FFmpeg::avcodec
        FFmpeg::avutil
        FFmpeg::swscale
        FFmpeg::swresample
)

# Définition pour le debug
target_compile_definitions(RecordingEngine
    PRIVATE
        $<$<CONFIG:Debug>:agk_DEBUG=1>
)
