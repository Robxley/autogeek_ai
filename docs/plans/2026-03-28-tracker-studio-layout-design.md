# Design Doc: Tracker Studio Layout & UI Evolution

**Date:** 2026-03-28
**Sujet:** Réorganisation de l'interface du Tracker Studio, intégration du docking par défaut, ajout d'icônes et de sélecteurs de fichiers.

## 1. Objectifs
Transformer le prototype actuel (fenêtres ImGui basiques) en un véritable "Studio" professionnel, avec un layout fenêtré par défaut modifiable par l'utilisateur.

## 2. Nouvelles Dépendances
- **Portable File Dialogs (pfd)** : Bibliothèque header-only (`portable-file-dialogs.h`) pour ouvrir des explorateurs natifs Windows (sélection de dossier d'export, ouverture de session).
- **IconsFontAwesome6** : Intégration des headers C++ et du fichier `.ttf` de FontAwesome 6 Free Regular/Solid pour remplacer les textes par des icônes nettes et professionnelles.

## 3. Architecture de l'Interface (Layout par Défaut)

Le layout sera implémenté via l'API `ImGui::DockBuilder` lors du premier lancement (ou réinitialisation).

### A. Panneau Gauche : Explorateur de Sessions (Session Library)
- Maintien d'une arborescence des sessions détectées.
- Bouton `[Icon Folder] Ouvrir` utilisant `pfd` pour pointer vers un lecteur externe ou configurer le dossier racine.

### B. Zone Centrale : Workspace (Live & Replay)
Divisée horizontalement (ou en onglets si l'utilisateur le préfère après le layout initial) :
- **Live Monitor** : Retour vidéo DXGI en direct, barre d'outils avec contrôle d'enregistrement (`[Icon Record] Start`, `[Icon Stop] Stop`).
- **Session Replayer** : Lecteur vidéo avec timeline interactive (`ImGui::SliderFloat` ou ImPlot) et boutons de contrôle temporels (`Play`, `Pause`, `Seek`).

### C. Panneau Droit : Configurations & Inspecteur
- **Recording Config** : Éditeur visuel pour `recording_config.json` (Bitrate, FPS, Audio On/Off). Bouton "Apply" qui redémarre l'Engine.
- **Studio Setup** : Réglages du client (Thèmes, dossiers de cache, résolutions de prévisualisation).

### D. Panneau Inférieur : Console & Timeline
- Séparé en onglets : Logs internes (spdlog -> ImGui) et Liste des événements bruts (JSONL) synchronisée avec la timeline du Replayer.

## 4. Implémentation Prévue
1. Fetch ou téléchargement de PFD et FontAwesome 6 dans `/third_party/` ou via CMake.
2. Mise à jour du système d'initialisation ImGui dans `main.cpp` pour fusionner la font par défaut (ex: Roboto) avec le ttf FontAwesome.
3. Implémentation du système `DockBuilder` au démarrage (si le fichier `imgui.ini` est absent).
4. Découpage du code UI actuel (monolithique) en sous-classes ou fonctions par "Panel" pour garder `main.cpp` lisible.
