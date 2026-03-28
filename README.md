# Autogeek AI 
```text
    _         _        ___            _       __   ___ 
   / \  _   _| |_ ___ / _ \ ___  ___ | | __  / /  |_ _|
  / _ \| | | | __/ _ \ | | |/ _ \/ _ \| |/ / / /_  | | 
 / ___ \ |_| | || (_) | |_| |  __/  __/   < / _ \_ | | 
/_/   \_\__,_|\__\___/ \___/ \___|\___|_|\_/_/ \_\___|
        Recording Engine & Tracker Studio
```

A modular, high-performance video, audio, and inputs capture solution designed for programmatic recording and synchronized replay. Written in modern C++20.

## 🌟 Overview

The Autogeek AI Engine provides a completely synchronized acquisition pipeline without the overhead of heavy desktop compositors. It natively captures:
- **Video:** Using **WinRT (`Windows.Graphics.Capture`)** for 4K/60+ FPS zero-delay zero-ban-risk capture.
- **Audio:** Using **WASAPI** Loopback to perfectly isolate process audio or record the whole system.
- **Inputs:** Using Win32 **Raw Inputs** and **SDL3 Gamepad** tracking to record exact user interactions (clicks, keyboard strokes, stick movements) alongside the video stream.

All data is recorded into a resilient `MKV` container via **FFmpeg (nvenc hw-accel)** and an `events.jsonl` (JSON Lines) stream.

## 🏗️ Project Architecture

The project is divided into three distinct targets:

1. **`RecordingEngine` (Static Library / C++20 Module)**  
   The core API. Asynchronous, multi-threaded (`std::jthread`), and queue-based (`MPMC`). Handles DXGI/WinRT, FFmpeg encoding, and Windows Input intercepting.
   
2. **`TrackerStudio` (GUI Application)**  
   A Dear ImGui / Vulkan based dashboard. Allows you to configure the engine, view a live preview hook, and browse past recordings with a synchronized video/input replayer.
   
3. **`TrackerCLI` / `SnakeSimulator` (Headless/Test Apps)**  
   A lightweight command-line interface for CI/CD environments, and a built-in game (`SnakeSimulator`) for testing latency and input hooks.

---

## 🛠️ Build Instructions

### Prerequisites
- **OS**: Windows 10/11 (Required for WinRT and WASAPI)
- **Compiler**: Visual Studio 2022 (MSVC v143 toolset)
- **CMake**: 3.28 or higher

*Dependencies like SDL3, FFmpeg, Dear ImGui, and Vulkan Headers are automatically downloaded via `FetchContent` during the CMake configure step.*

### Compilation

```bash
# 1. Create a build directory
mkdir build && cd build

# 2. Configure the project
cmake ..

# 3. Build the project (Release mode recommended for performance)
cmake --build . --config Release
```

### Running the Applications

After building, the executables will be located in `build/src/<AppName>/Release/`.

```bash
# Launch the main GUI Studio
./build/src/TrackerStudio/Release/TrackerStudio.exe

# Or use the CLI for headless recording (reads tracker_config.json)
./build/src/TrackerCLI/Release/TrackerCLI.exe
```

---

## 👨‍💻 Coding Standards & Contributions

This project strictly adheres to robust modern C++ practices:

- **C++20 Modules**: Internal implementation details are hidden behind `export module RecordingEngine;` rather than traditional headers where applicable.
- **RAII (Resource Acquisition Is Initialization)**: All memory management must be safe. `std::unique_ptr` with custom deleters is strictly enforced for C APIs (e.g., `av_frame_free`, `SDL_DestroyWindow`). Raw pointers doing manual destruction (`delete`, `free`) are forbidden.
- **Universal Logging**: The use of `std::cout`, `std::cerr`, or `printf` is strictly forbidden. Developers must use the integrated `spdlog` macros (`AGK_INFO`, `AGK_ERROR`, etc.) to ensure logs are written to disk cleanly.
- **COM Safety**: All Windows APIs interactions (WinRT, DXGI) should use `Microsoft::WRL::ComPtr` or C++/WinRT native smart pointers.

## 📄 Documentation

For an in-depth explanation of the multithreading queue system and the JSON configuration file schemas, see the technical specifications in `specification.md`.
