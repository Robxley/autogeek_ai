# tests/RecordingEngine/CMakeLists.txt

# Créer l'exécutable de tests
add_executable(RecordingEngine_Tests
    test_ConfigSystem.cpp
    test_SyncSystem.cpp
    test_DXGICapture.cpp
    test_LogSystem.cpp
    test_EncoderModule.cpp
)

# Liens avec GTest et le module à tester
target_link_libraries(RecordingEngine_Tests
    PRIVATE
        GTest::gtest_main
        RecordingEngine
        d3d11
        dxgi
)

# Découvrir les tests automatiquement
gtest_discover_tests(RecordingEngine_Tests)

# Activer C++20 pour les tests
target_compile_features(RecordingEngine_Tests PRIVATE cxx_std_20)

# Copie des DLLs
agk_copy_dlls(RecordingEngine_Tests)
