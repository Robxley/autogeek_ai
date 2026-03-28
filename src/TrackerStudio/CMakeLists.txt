# src/TrackerStudio/CMakeLists.txt

add_executable(TrackerStudio main.cpp)

# Liens avec le module RecordingEngine et les dépendances GUI
target_link_libraries(TrackerStudio
    PRIVATE
        RecordingEngine
        imgui::imgui
        implot::implot
        SDL3::SDL3
        Vulkan::Vulkan
)

# Activer les modules C++20 pour la cible
target_compile_features(TrackerStudio PRIVATE cxx_std_20)

# Copie des DLLs
agk_copy_dlls(TrackerStudio)
