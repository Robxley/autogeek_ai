# Design Document : Infrastructure & Core RecordingEngine

**Date** : 2026-03-28
**Sujet** : Finalisation de l'infrastructure CMake et architecture du Core (`RecordingEngine`)
**Statut** : Validé par l'utilisateur

## 1. Objectifs de l'Infrastructure

L'infrastructure doit supporter le développement d'un moteur de capture haute performance en C++20 sur Windows, en utilisant les modules C++20 et en automatisant la gestion des dépendances.

## 2. Structure du Projet (Pitchfork Layout)

Le projet suivra une séparation stricte entre interfaces publiques et implémentations privées :

```text
/
├── include/
│   └── agk/
│       └── RecordingEngine/
│           └── IRecordingEngine.hpp  <-- Interface publique (Abstraite)
├── src/
│   ├── RecordingEngine/
│   │   ├── RecordingEngine.ixx       <-- Interface du Module C++20
│   │   ├── RecordingEngineImpl.cpp   <-- Implémentation concrète
│   │   ├── ConfigSystem.cpp          <-- Gestionnaire de configuration
│   │   └── SyncSystem.cpp            <-- Gestionnaire de temps
│   ├── TrackerCLI/                   <-- Application Console
│   └── TrackerStudio/                <-- Application GUI (ImGui/Vulkan)
├── cmake/
│   └── Dependencies.cmake            <-- Centralisation des FetchContent
├── tests/
│   └── RecordingEngine/              <-- Tests unitaires (GTest)
└── docs/
    └── plans/                        <-- Documents de conception et plans
```

## 3. Gestion des Dépendances (FetchContent)

Toutes les dépendances seront gérées via `FetchContent` pour garantir un environnement de build reproductible sans installation manuelle (hormis le Vulkan SDK).

### FFmpeg
- **Méthode** : Téléchargement automatique des binaires pré-compilés (Shared/Dev) depuis `gyan.dev` via un script CMake personnalisé déclenché par `FetchContent`.
- **Intégration** : Création de cibles importées (`FFmpeg::avcodec`, `FFmpeg::avformat`, etc.) pour `target_link_libraries`.
- **Optimisation** : Support hardware NVENC/AMF/QSV activé par défaut.

### Autres
- **Vulkan** : `find_package(Vulkan REQUIRED)`.
- **SDL3**, **Dear ImGui**, **ImPlot** : Compilation depuis les sources via `FetchContent`.
- **spdlog**, **nlohmann_json** : Compilation depuis les sources via `FetchContent`.
- **GoogleTest** : Intégration pour les tests unitaires.

## 4. Architecture du Core (RecordingEngine)

- **Modulaire** : Utilisation des **Modules C++20** (`export module RecordingEngine;`).
- **RAII & Performance** :
  - `std::jthread` pour la gestion automatique du cycle de vie des threads.
  - Ring Buffers (ou MPMC) pour le transfert de données "Zero-Copy" entre Threads (Capture -> Encodeur).
- **Synchronisation** :
  - Horloge `steady_clock`.
  - Timestamping précis par frame index pour permettre une relecture synchronisée parfaite dans `TrackerStudio`.
- **Résilience** :
  - Format **MKV par défaut** pour éviter la corruption de fichier en cas de crash.
  - Flush régulier des `events.jsonl`.

## 5. Plan de Validation

- **Tests de Compilation** : Validation de la détection des Headers et Libs FFmpeg.
- **Tests Unitaires** :
  - `ConfigSystem` : Validation du parsing JSON et des valeurs par défaut.
  - `SyncSystem` : Validation du calcul des offsets temporels après une pause.
  - `RecordingEngine` : Validation de l'initialisation des modules DXGI/WASAPI (Mocks).

---
> [!IMPORTANT]
> Ce design est la fondation des phases 1 et 2 du projet. Toute modification majeure de l'infrastructure CMake devra être ré-évaluée via le process de brainstorming.
