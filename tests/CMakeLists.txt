# tests/CMakeLists.txt

# Inclure GoogleTest (déjà disponible via FetchContent_MakeAvailable dans Dependencies.cmake)
include(GoogleTest)

# Ajouter les sous-dossiers de tests
add_subdirectory(RecordingEngine)
