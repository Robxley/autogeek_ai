# src/SnakeSimulator/CMakeLists.txt

add_executable(SnakeSimulator
    main.cpp
    SnakeGame.cpp
    SnakeGame.hpp
    EventSimulator.hpp
)

# Utilisation des modules pour la cohérence même si c'est une app simple
target_compile_features(SnakeSimulator PRIVATE cxx_std_20)

# Liens avec SDL3 et RecordingEngine
target_link_libraries(SnakeSimulator PRIVATE SDL3::SDL3 RecordingEngine)

# Copie des DLLs FFmpeg et SDL3
agk_copy_dlls(SnakeSimulator)

# Dossiers d'inclusion
target_include_directories(SnakeSimulator PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Optionnel: Cacher la console sur Windows en Release
# set_target_properties(SnakeSimulator PROPERTIES WIN32_EXECUTABLE TRUE)
