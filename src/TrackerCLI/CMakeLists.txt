# src/TrackerCLI/CMakeLists.txt

add_executable(TrackerCLI main.cpp)

# Liens avec le module RecordingEngine
target_link_libraries(TrackerCLI
    PRIVATE
        RecordingEngine
)

# Activer les modules C++20 pour la cible
target_compile_features(TrackerCLI PRIVATE cxx_std_20)

# Copie des DLLs
agk_copy_dlls(TrackerCLI)
