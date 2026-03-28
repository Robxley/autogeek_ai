# src/TrackerStudio/CMakeLists.txt

add_executable(TrackerStudio 
    main.cpp
    StudioConfig.cpp
    LiveMonitor.cpp
    SessionReplayer.cpp
    StudioApp.cpp
    Widgets/SessionExplorer.cpp
    Widgets/RecordingConfigUI.cpp
    Widgets/AppLogUI.cpp
)

# Liens avec le module RecordingEngine et les dépendances GUI
target_link_libraries(TrackerStudio
    PRIVATE
        RecordingEngine
        imgui::imgui
        implot::implot
        SDL3::SDL3
        Vulkan::Vulkan
        portable_file_dialogs
)

# Activer les modules C++20 pour la cible
target_compile_features(TrackerStudio PRIVATE cxx_std_20)

# Copie des DLLs
agk_copy_dlls(TrackerStudio)
