# Plan d'implémentation : Infrastructure CMake & Core Foundation

**Objectif :** Finaliser l'infrastructure de build modulaire et implémenter les systèmes de base (Config, Sync) du moteur de capture avec une couverture de tests unitaires.

**Architecture :** 
- Build modulaire avec CMake et FetchContent.
- Layout Pitchfork pour la séparation include/src.
- Architecture basée sur des interfaces (IRecordingEngine) pour le découplage.

**Stack Technique :** 
- C++20 (Modules, jthread, functional)
- CMake 3.28+
- FFmpeg (binaires via FetchContent)
- SDL3, spdlog, nlohmann_json, GoogleTest

---

### Tâche 1 : Mise en place de la structure Pitchfork

**Fichiers concernés :**
- Créer : `include/agk/RecordingEngine/IRecordingEngine.hpp`
- Créer : `src/RecordingEngine/RecordingEngine.ixx`
- Déplacer/Modifier : `src/RecordingEngine/RecordingEngine.cpp` -> `src/RecordingEngine/RecordingEngineImpl.cpp`

**Étape 1 : Créer l'interface abstraite**
```cpp
// include/agk/RecordingEngine/IRecordingEngine.hpp
namespace agk {
    class IRecordingEngine {
    public:
        virtual ~IRecordingEngine() = default;
        virtual bool Initialize() = 0;
        virtual void Start() = 0;
        virtual void Stop() = 0;
    };
}
```

**Étape 2 : Créer l'interface du module C++20**
```cpp
// src/RecordingEngine/RecordingEngine.ixx
export module RecordingEngine;
export import "agk/RecordingEngine/IRecordingEngine.hpp";

export namespace agk {
    // Factory function
    export IRecordingEngine* CreateEngine();
}
```

---

### Tâche 2 : Centralisation des dépendances (FetchContent)

**Fichiers concernés :**
- Créer : `cmake/Dependencies.cmake`
- Modifier : `CMakeLists.txt` (racine)

**Étape 1 : Rédiger le script de dépendances**
```cmake
# cmake/Dependencies.cmake
include(FetchContent)

# GTest
FetchContent_Declare(googletest URL https://github.com/google/googletest/archive/refs/heads/main.zip)
FetchContent_MakeAvailable(googletest)

# spdlog, nlohmann_json, SDL3 (déjà esquissés, à déplacer ici)
```

**Étape 2 : Nettoyer le CMakeLists.txt racine**
Supprimer les appels FetchContent directs et inclure `cmake/Dependencies.cmake`.

---

### Tâche 3 : Intégration automatisée de FFmpeg

**Fichiers concernés :**
- Créer : `cmake/FFmpegFetch.cmake`

**Étape 1 : Script de téléchargement des binaires Windows**
```cmake
# cmake/FFmpegFetch.cmake
# Téléchargement de FFmpeg (Gyan.dev full shared build)
# Extraction et création de cibles IMPORTED (avcodec, avformat, etc.)
```

---

### Tâche 4 : Implémentation du ConfigSystem (TDD)

**Fichiers concernés :**
- Créer : `src/RecordingEngine/ConfigSystem.hpp`
- Créer : `src/RecordingEngine/ConfigSystem.cpp`
- Créer : `tests/RecordingEngine/test_ConfigSystem.cpp`

**Étape 1 : Écrire le test unitaire**
```cpp
TEST(ConfigSystem, LoadDefaultConfig) {
    agk::ConfigSystem config;
    EXPECT_EQ(config.GetVideoFormat(), "mkv");
}
```

**Étape 2 : Implémentation minimale**
Définir la struct `Config` et le parser utilisant `nlohmann/json`.

---

### Tâche 5 : Implémentation du SyncSystem (TDD)

**Fichiers concernés :**
- Créer : `src/RecordingEngine/SyncSystem.hpp`
- Créer : `tests/RecordingEngine/test_SyncSystem.cpp`

**Étape 1 : Écrire le test pour la gestion des pauses**
```cpp
TEST(SyncSystem, HandlePauseOffset) {
    agk::SyncSystem sync;
    sync.Start();
    // simuler pause...
    // vérifier que le frame_index reste cohérent
}
```
