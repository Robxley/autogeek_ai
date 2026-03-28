# Spécification Technique : Système de Capture Vidéo, Audio, Inputs et Gamepad (C++20 Modules)

## 1. Objectif du Projet

Développer une solution modulaire de capture haute performance permettant d'enregistrer le flux vidéo et audio d'une application cible ainsi que les métadonnées d'interaction (Clavier, Souris, Manette) avec une synchronisation parfaite.

## 2. Architecture du Projet (Structure Tripartite)

Le projet est divisé en trois entités distinctes :

### 2.1. RecordingEngine (Bibliothèque / Module Core)

Le cœur du système, indépendant de toute interface utilisateur graphique.

* **Responsabilités** : Capture DXGI, Capture Audio WASAPI, gestion des Raw Inputs, polling Gamepad, encodage FFmpeg hardware, gestion du JSONL, **gestion des raccourcis globaux**, et gestion du cycle de vie de la cible (Start/Pause/Resume automatique).

* **Format** : Module C++20 (`export module RecordingEngine;`).

### 2.2. TrackerCLI (Application Console)

Interface légère en ligne de commande pour lancer des enregistrements via `config.json` sans surcharge graphique. Idéal pour l'automatisation.

### 2.3. TrackerStudio (Application GUI - ImGui)

Outil complet de configuration, de monitoring en temps réel et de relecture synchronisée (Timeline vidéo + inputs).

## 3. Stack Technique et Bibliothèques

### 3.1. Cœur du Moteur (RecordingEngine)

* **Vidéo (Capture)** : **Windows Desktop Duplication API (DXGI)**. Accès direct au tampon GPU, méthode la plus performante pour les jeux.

* **Audio (Capture)** : **WASAPI (Windows Audio Session API)**. Mode *Loopback* pour capturer le son du bureau/jeu, et mode capture standard pour le microphone.

* **Vidéo/Audio (Encodage)** : **FFmpeg** (libavcodec). Utilisation prioritaire des encodeurs hardware vidéo (`h264_nvenc`, `h264_amf`) et audio (`aac`).

* **Inputs (KB/Mouse)** : **Raw Input (Win32 API)**. Permet la capture globale des entrées même si l'application cible a le focus (contrairement à SDL).

* **Inputs (Gamepad)** : **SDL_GameController** (via SDL2). Performant et gère nativement le polling en arrière-plan.

* **Contrôle (Hotkeys)** : **`RegisterHotKey` (Win32)** pour l'interception des commandes clavier système.

* **Format de Données** : **nlohmann_json**.

* **Logging** : **spdlog** (asynchrone).

### 3.2. Interface Utilisateur (TrackerStudio)

* **Framework UI** : **HelloImGui** (Base ImGui).

* **Backend de Rendu** : **SDL2 + Vulkan**.

* **Rendu de Texte & Icones** :

  * **FreeType** (intégré à ImGui).

  * `fa-solid-900.ttf` (Font Awesome pour les icônes).

  * `NotoColorEmoji.ttf` (Support des emojis).

  * Intégration des icônes système Windows.

* **Visualisation** : **ImPlot** pour les graphiques de moteurs, les waveforms audio et mouvements souris.

## 4. Modèle de Données et Synchronisation

### 4.1. Stockage (Résilience aux crashs)

Dossier session unique :

```
/session_YYYYMMDD_HHMMSS/
  ├── video/capture.mkv      (MKV préféré au MP4 pour éviter la corruption en cas de crash)
  ├── events.jsonl           (JSON Lines : un événement par ligne pour streaming sur disque)
  ├── session_info.json      (Métadonnées : résolution, FPS, specs)
  └── config_snapshot.json



```

### 4.2. Synchronisation et Contexte

* **Référence** : `std::chrono::steady_clock` démarré à l'acquisition de la première frame.

* **Indexation** : Chaque événement d'entrée est lié à un `frame_index` calculé pour permettre un "seek" précis dans le Studio.

* **Contexte Focus et Coordonnées (DPI Awareness)** :

  * L'application doit être déclarée **Per-Monitor DPI Aware** (via le manifest ou `SetProcessDpiAwareness`).

  * Les coordonnées de la souris capturées par Raw Input doivent être rigoureusement traduites en coordonnées locales à la fenêtre cible (`ScreenToClient`), en compensant l'échelle DPI, afin d'assurer que les positions `x` et `y` dans le `events.jsonl` correspondent aux pixels exacts du jeu.

  * Booléen `target_has_focus` (vérifié via `GetForegroundWindow()`) pour distinguer les actions in-game des actions hors-jeu.

## 5. Modèle Multithreading (C++20)

Utilisation intensive de `std::jthread` et `std::stop_token` :

* **Thread UI** : Rendu Vulkan / ImGui.

* **Thread Capture Vidéo** : Boucle DXGI haute priorité.

* **Thread Capture Audio** : Écoute asynchrone des buffers WASAPI.

* **Thread Input** : Pompe à messages Raw Input (Win32), polling SDL et écoute des Hotkeys.

* **Thread Encoder** : Queue asynchrone MPMC vers l'encodeur FFmpeg. Intègre une stratégie de **Backpressure** : si la file dépasse la limite (ex: 60 frames), le thread de capture doit "dropper" (jeter) les nouvelles frames vidéo pour éviter la saturation de la RAM, tout en inscrivant un avertissement de "Frame Drop" dans les logs.

## 6. Schéma de Configuration (recording_config.json)

Ce chapitre détaille les paramètres interprétés par le `RecordingEngine`.

### 6.1. Structure du fichier JSON

```
{
  "target": {
    "mode": "window",
    "process_name": "game.exe",
    "window_title": "My Awesome Game",
    "monitor_index": 0,
    "include_cursor": true,
    "wait_for_target": true,
    "auto_pause_on_minimize": true,
    "auto_resume_on_restore": true,
    "auto_stop_on_close": false
  },
  "hotkeys": {
    "enabled": true,
    "start_stop": "Ctrl+Shift+R",
    "pause_resume": "Ctrl+Shift+P",
    "add_marker": "Ctrl+Shift+M"
  },
  "recording": {
    "video": {
      "target_fps": 60,
      "width": 1920,
      "height": 1080,
      "use_source_resolution": true,
      "bitrate_kbps": 8000,
      "encoder": "h264_nvenc",
      "format": "mkv"
    },
    "audio": {
      "capture_system": true,
      "capture_mic": false,
      "system_device": "default",
      "mic_device": "default",
      "audio_bitrate_kbps": 192
    },
    "inputs": {
      "enabled": true,
      "capture_keyboard": true,
      "capture_mouse": true,
      "capture_gamepad": true,
      "mouse_sampling_rate_ms": 10,
      "gamepad_polling_rate_ms": 8,
      "gamepad_deadzone": 0.1,
      "raw_input_mode": true
    }
  },
  "storage": {
    "base_output_path": "./recordings",
    "video_subfolder": "video",
    "events_filename": "events.jsonl"
  },
  "system": {
    "thread_priority": "high",
    "gpu_acceleration": true,
    "internal_buffer_size": 60,
    "drop_frames_on_buffer_full": true
  }
}

```

### 6.2. Détails des paramètres interprétés

| **Paramètre** | **Rôle pour le RecordingEngine** | 
| `target.mode` | Définit si le moteur doit capturer une fenêtre (`window`), un processus (`process`) ou un écran entier (`monitor`). | 
| `target.wait_for_target` | Si `true`, le moteur reste en veille jusqu'à détection de la cible avant de démarrer le flux. | 
| `target.auto_pause_on_minimize` | Met en pause automatique la capture vidéo/audio/inputs si la cible est réduite (minimize). | 
| `hotkeys.start_stop` | Combinaison de touches (Win32) globale permettant de démarrer ou d'arrêter l'enregistrement sans focus sur TrackerStudio. | 
| `hotkeys.add_marker` | Injecte un événement de type `MARKER` dans `events.jsonl` pour retrouver facilement un moment précis (ex: un bug). | 
| `video.target_fps` | Cadence de la boucle de capture DXGI et base de temps pour l'encodage FFmpeg. | 
| `video.encoder` | Sélection de l'ID du codec FFmpeg (ex: `h264_nvenc`). Format cible MKV pour la sécurité. | 
| `audio.capture_system` | Active l'initialisation de WASAPI en mode Loopback pour capturer les sons du bureau/jeu. | 
| `inputs.raw_input_mode` | Active l'usage de `RegisterRawInputDevices` pour intercepter les flux matériels hors-focus. | 
| `inputs.gamepad_polling_rate_ms` | Fréquence de lecture de l'état `SDL_GameController`. | 
| `storage.events_filename` | Utilisation du format JSONL (JSON Lines) pour éviter la perte de données en cas de crash. | 
| `system.internal_buffer_size` | Nombre de frames maximales stockées en RAM dans la file de production avant encodage. | 
| `system.drop_frames_on_buffer_full` | Si l'encodeur est surchargé, ignore les nouvelles frames capturées pour prévenir l'épuisement de la RAM (Backpressure). | 

## 7. Contraintes de Build et d'Exécution

* **Compilateur** : MSVC 2022 (v143) ou Clang 16+.

* **Build System** : CMake 3.28+.

* **Modules** : Migration complète vers `import` pour tous les composants internes du projet.

* **Privilèges** : L'exécutable doit souvent être lancé en tant qu'**Administrateur (UAC)** pour permettre aux Raw Inputs d'intercepter les données lorsque des jeux protégés ou lancés en admin ont le focus.

## 8. Plan de Développement Détaillé

Ce plan structuré par phases servira de feuille de route pour l'implémentation. Il inclut l'architecture conceptuelle des classes du `RecordingEngine`.

### 8.1. Architecture des Classes (Diagramme)

```
classDiagram
    class IRecordingEngine {
        <<interface>>
        +Initialize(configPath)
        +Start()
        +Stop()
        +Pause()
        +Resume()
        +GetStatus()
    }
    
    class RecordingEngineImpl {
        -ConfigSystem m_config
        -JobSystem m_jobSystem
        -SyncSystem m_sync
        -TargetTracker m_targetTracker
        -SessionManager m_sessionManager
    }
    
    IRecordingEngine <|-- RecordingEngineImpl
    
    %% Core Systems
    class ConfigSystem {
        +LoadFromJson(path)
        +GetSettings()
    }
    class SyncSystem {
        +StartClock()
        +GetRelativeTimeMs()
        +GetFrameIndex()
        +HandlePauseOffset()
    }
    class JobSystem {
        +QueueVideoFrame(frame)
        +QueueAudioBuffer(buffer)
        +QueueInputEvent(event)
    }
    class TargetTracker {
        +UpdateLifecycle()
        +HasFocus() bool
        +ScreenToClientCoords(x, y)
    }
    class SessionManager {
        +CreateSessionTree()
        +WriteMetadata()
    }

    %% Acquisition Modules
    class DXGICaptureModule {
        +StartCaptureLoop(JobSystem)
    }
    class AudioWASAPIModule {
        +StartCaptureLoop(JobSystem)
    }
    class RawInputModule {
        +RegisterDevices()
        +MessagePumpLoop(JobSystem)
    }
    class GamepadModule {
        +PollLoop(JobSystem)
    }
    class HotkeyModule {
        +RegisterGlobalHotkeys()
    }

    %% Processing & Output
    class FFmpegEncoderModule {
        +ProcessVideoQueue()
        +ProcessAudioQueue()
        +MuxAndWrite()
    }
    class EventSerializer {
        +ProcessEventQueue()
        +StreamToJsonl()
    }

    %% Relations
    RecordingEngineImpl *-- ConfigSystem
    RecordingEngineImpl *-- SyncSystem
    RecordingEngineImpl *-- JobSystem
    RecordingEngineImpl *-- TargetTracker
    RecordingEngineImpl *-- SessionManager
    
    RecordingEngineImpl --> DXGICaptureModule
    RecordingEngineImpl --> AudioWASAPIModule
    RecordingEngineImpl --> RawInputModule
    RecordingEngineImpl --> GamepadModule
    RecordingEngineImpl --> HotkeyModule
    
    RecordingEngineImpl --> FFmpegEncoderModule
    RecordingEngineImpl --> EventSerializer

```

### Phase 1 : Infrastructure & CMake

* $$
  
  $$

   Configuration du fichier `CMakeLists.txt` racine (activation C++20, Modules).

* $$
  
  $$

   Intégration des dépendances (via `FetchContent` ou `vcpkg` : spdlog, nlohmann_json, FFmpeg, SDL2, Vulkan, HelloImGui, ImPlot).

* $$
  
  $$

   Création et configuration de la cible `RecordingEngine` (Static Lib / Module).

* $$
  
  $$

   Création de la cible `TrackerCLI` (Executable).

* $$
  
  $$

   Création de la cible `TrackerStudio` (Executable).

* $$
  
  $$

   Intégration du framework de test unitaire (GoogleTest ou Catch2).

### Phase 2 : Architecture Core (`RecordingEngine` - Socle)

* $$
  
  $$

   **`ConfigSystem`** : Parsing de `recording_config.json`, validation des types et exposition sécurisée (struct).

* $$
  
  $$

   **`JobSystem`** : Création du gestionnaire de threads (`std::jthread`). Implémentation des files lock-free MPMC (Multi-Producer Multi-Consumer) pour les Data Transfer Objects : `VideoFrameDTO`, `AudioBufferDTO`, `InputEventDTO`.

* $$
  
  $$

   **`SyncSystem`** : Encapsulation de `std::chrono::steady_clock`. Calcul dynamique du `frame_index`. Implémentation de la compensation temporelle lors des pauses (`HandlePauseOffset`).

* $$
  
  $$

   **`TargetTracker`** : Logique de détection de processus (PID), de Handles de fenêtres (HWND). Gestion de l'état (Minimize/Restore/Close) pour piloter l'Auto-Pause/Resume.

* $$
  
  $$

   *Tests Unitaires* : Mock du `ConfigSystem` et stress-test des files lock-free du `JobSystem`.

### Phase 3 : Modules d'Acquisition (Inputs, Vidéo & Audio)

* $$
  
  $$

   **`RawInputModule`** : Boucle Win32 (`GetMessage`). Gestion clavier/souris. Injection de la logique de DPI Awareness (`ScreenToClientCoords` via le `TargetTracker`) et de l'état de focus (`target_has_focus`).

* $$
  
  $$

   **`GamepadModule`** : Boucle de polling (`SDL_GameController`). Gestion de la *deadzone* configurable et de la fréquence de rafraichissement.

* $$
  
  $$

   **`HotkeyModule`** : Enregistrement (`RegisterHotKey`) et écoute des commandes globales (Start, Pause, Marker).

* $$
  
  $$

   **`DXGICaptureModule`** : Initialisation DXGI, copie "Zero-Copy" si possible, et push des textures dans `JobSystem`. Application de la backpressure (Drop frame si file pleine).

* $$
  
  $$

   **`AudioWASAPIModule`** : Initialisation COM, création des clients WASAPI Loopback (Système) et Capture (Mic), mixage basique et push des buffers PCM.

* $$
  
  $$

   *Tests Unitaires* : Mocks d'interruption Win32 pour valider le parsing des Raw Inputs.

### Phase 4 : Encodage et Sérialisation

* $$
  
  $$

   **`SessionManager`** : Génération de l'arborescence `/session_YYYYMMDD_HHMMSS/`, copie de la configuration et écriture de `session_info.json`.

* $$
  
  $$

   **`EventSerializer`** : Consommation de la queue d'`InputEventDTO` depuis le `JobSystem` et écriture asynchrone (append) dans `events.jsonl`. Implémentation du flush régulier.

* $$
  
  $$

   **`FFmpegEncoderModule`** : Consommation des queues `VideoFrameDTO` et `AudioBufferDTO`. Configuration des contextes libavcodec (H264/NVENC, AAC). Muxing et écriture dans le `.mkv`. Gestion transparente des "trous" de timestamps lors d'une pause.

* $$
  
  $$

   *Tests d'Intégration* : Scénario "Headless" complet générant des vecteurs de test (10 sec de frames factices + inputs) pour vérifier la validité du MKV et du JSONL.

### Phase 5 : Applications Clientes (CLI & Studio)

* $$
  
  $$

   **TrackerCLI** : Implémentation du `main()`, parsing CLI (`--config`, `--output`), instanciation de `IRecordingEngine` et boucle d'attente (Hook des signaux Ctrl+C).

* $$
  
  $$

   **TrackerStudio (Init)** : Setup de HelloImGui (Vulkan + SDL2). Chargement asynchrone des polices (FontAwesome, NotoColorEmoji).

* $$
  
  $$

   **TrackerStudio (Dashboard)** : Création des fenêtres ImGui : Statuts matériels, Vu-mètres audio en temps réel, statistiques d'encodage (FPS in/out, frame drops), contrôles (REC/PAUSE/STOP).

* $$
  
  $$

   **TrackerStudio (Replayer Core)** : Intégration d'un mini-décodeur FFmpeg pour extraire les frames du `.mkv` et les charger en tant que `ImTextureID` Vulkan.

* $$
  
  $$

   **TrackerStudio (Timeline)** : Intégration d'`ImPlot`. Affichage horizontal de la vidéo synchronisée avec les lignes d'événements (Clics, Touches, PAD, Marqueurs). Outils de zoom et seek temporel interactif.

### Phase 6 : Polissage, Profiling et Validation E2E

* $$
  
  $$

   Profiling Mémoire/CPU : Traque des fuites avec Valgrind/Visual Studio Profiler. Optimisation des allocations (Object Pooling pour les DTOs).

* $$
  
  $$

   Profiling de Latence : Vérification rigoureuse de la synchronisation A/V (Audio/Video Sync) et A/V/Input (Moins de 16ms de dérive tolérée).

* $$
  
  $$

   Documentation : Complétion de la documentation des modules C++ (Doxygen format).

* $$
  
  $$

   Tests E2E (End-to-End) : Scénario réel complet : Lancement cible -> Enregistrement -> Alt-Tab (Pause auto) -> Retour (Resume) -> Hotkey Marker -> Stop -> Validation dans le Studio.
