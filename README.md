<div align="center">
  <h1>X-Ray Monolith: Next Gen</h1>

  <h4>Upgraded X-Ray Monolith Engine for S.T.A.L.K.E.R. Anomaly 1.5.3</h4>

  <p>
    <a href="./License.txt">
      <img src="https://img.shields.io/badge/License-Non--commercial-red.svg" alt="License" />
    </a>
    <a href="https://github.com/CnRJay/xray-monolith-gc64/releases/latest">
      <img src="https://img.shields.io/github/v/release/CnRJay/xray-monolith-gc64?include_prereleases&label=Release" alt="Latest release" />
    </a>
    <a href="https://github.com/CnRJay/xray-monolith-gc64/releases">
      <img src="https://img.shields.io/github/downloads/CnRJay/xray-monolith-gc64/total?label=Downloads" alt="All downloads" />
    </a>
  </p>
</div>


> DISCLAIMER: This fork just exists to pull in the latest changes from Demonized Modded EXEs, until the author (CnRJay) updates the project. I'm unlikely going to fix existing issues and such.

## Overview

**X-Ray Monolith: Next Gen** is a WIP fork of the **X-Ray Monolith Engine**, designed specifically for heavy modpacks (like G.A.M.M.A.) that run hundreds of scripts. The primary goal is to provide maximum stability and memory availability.

This project introduces a modernized memory and threading architecture to break the 15-year-old limitations of the original X-Ray engine, virtually eliminating "Out of Memory" crashes during long sessions.

> [!TIP]
> **G.A.M.M.A. Users**: This engine is **Plug and Play**! If you are playing S.T.A.L.K.E.R. G.A.M.M.A., all necessary dependencies (including 3DSS) are already configured. You are ready to play immediately after installation.

## Quick start

The latest release of the engine can be downloaded on the [releases page](https://github.com/CnRJay/xray-monolith-gc64/releases).

## Features

### Core Architecture
- **LuaJIT GC64 Implementation**: 
  - Enables 64-bit pointers in Lua, allowing the game to utilize **10GB+ of RAM** for scripts.
  - Fixes the standard ~2GB limit crash in Anomaly.
- **Intel TBB Integration**:
  - Replaces standard Windows memory allocator with **Intel Threading Building Blocks (TBB)**.
  - Reduces memory fragmentation and micro-stutters.
- **Hybrid Multithreading**:
  - Upgraded systems to improve 1% lows and FPS in CPU-bound situations.
- **Faster Load Times**: 
  - Optimized file system and asset loading pipelines to significantly reduce time loading save games and level transitions.
- **True PIP (Picture-in-Picture)**: 
  - Native engine support for dual rendering scopes increasing immersion.
  - **Requirement**: Requires **3DSS (3D Shader Scopes)** to function correctly.

### Monolith Features
- **DLTX (Expanded LTX Support)**:
  - Override sections without crashing.
  - Support for `@[newsection]` creation.
  - Advanced parameter manipulation (`>name = add`, `<name = remove`).
- **DXML**: Allows Lua scripts to modify XML UI files on the fly.
- **Shader Support**: Full compatibility with **Screen Space Shaders (SSS)**, **Beef's NVG**, and **Shader Scopes**.
- **Debug Tools**: Integrated LuaPanda support and ImGui extensions.
- **Fixes**: Includes fixes for animation smoothing, collision bugs, and the "1000 save" limit.

## Minimal system requirements

- **OS**: Windows 10/11 (1903 Update or newer)
- **CPU**: Supports SSE2 and newer instructions
- **RAM**: 8 GB+ recommended for heavy modpacks
- **GPU**: Support for Shader Model 3.0 or newer
- **DirectX**: 9.0c or newer

## Requirements

**For Launching:**
- [Visual C++ Redistributable 2022 (All-in-One)](https://www.techpowerup.com/download/visual-c-redistributable-runtime-package-all-in-one/)
- S.T.A.L.K.E.R. Anomaly 1.5.3 installation
- **3DSS (3D Shader Scopes)** (Included in G.A.M.M.A., required for other modpacks)

**For Development:**
- [Visual Studio 2022 Community Edition](https://visualstudio.microsoft.com/vs/community/)
  - Workload: Desktop development with C++
- [Git](https://git-scm.com/downloads)

## Installation

1.  **Backup**: Backup your existing `bin` folder in your Anomaly directory.
2.  **Download**: Get the **Release** archive from the [releases page](https://github.com/dtrail/xray-monolith-gc64/releases).
3.  **Extract**: Extract the contents (the `.exe` and `.dll` files) into your `ANOMALY/bin` folder.
    *   *Note: Ensure the new `lua51.dll` from this download is in the bin folder. The engine will not run with the standard Anomaly version.*
4.  **Clear Cache (CRITICAL)**: Navigate to `appdata/shaders_cache` and **delete the folder**.
    *   *Failure to do this may result in a black screen upon loading a saved game.*
5.  **Launch**: Run the game using the new executable.

## Building

The project is configured for a "One-Click Build" workflow in Visual Studio 2026.

### Option 1: Visual Studio IDE
1.  Open `engine-vs2022.sln` in Visual Studio 2026.
2.  Select the Solution Configuration (`DX11` and `DX11-AVX`).
3.  Select the Solution Platform: **x64**.
4.  Right-click **Solution 'Engine'** in the explorer and select **Build Solution**.

*The build process will automatically compile LuaJIT with GC64 support, link TBB libraries, and output the final executables.*

### Option 2: Batch Script
You can also use the provided batch script to build multiple configurations at once.
Run the following command in a terminal with access to MSBuild (or use the script directly if `vswhere` is installed):
```sh
.\src\batch_build.bat
```

### Output Location
- Executables and PDBs: `_build/_game/bin_dbg/`
- LuaJIT Library (`lua51.dll`): `_build/_game/bin_dbg/x64/Release/` (or `Release-AVX`)

## Troubleshooting & FAQ

**Q: My RAM usage is very high (6GB - 10GB+). Is this a memory leak?**
**A:** **No.** This is intentional. The new allocator aggressively caches memory to prevent stuttering, and the GC64 Lua implementation uses more memory per object to ensure stability.

**Q: The screen is black but I can see the HUD.**
**A:** You did not delete your `shaders_cache`. Delete the folder in `appdata` and restart.

**Q: I am crashing with LNK1120 or "Entry Point Not Found".**
**A:** You likely have mismatched DLLs. Ensure you copied `lua51.dll` from the release zip into your `bin` folder and overwrote the old one.

## Contributing

We welcome contributions to improve performance and stability! Please follow these guidelines.

### Guidelines
1.  **Fork & Clone**: Create a fork of the repository and clone it locally.
2.  **Branching**: Create a new branch for your feature or fix (e.g., `git checkout -b fix/memory-leak`).
3.  **Thread Safety (CRITICAL)**:
    *   **Do NOT** execute Lua callbacks inside TBB parallel loops (causes deadlocks).
    *   **Do NOT** write to global physics/sound states from worker threads without locking.
    *   Verify thread safety for any new threaded system.

### Pull Request Process
- Ensure code compiles.
- Verify game launches and runs without freezing.
- Submit PR with clear description.

## License

Contents of this repository are licensed under a custom GSC Game World proprietary license for non-commercial use. See the [License.txt](./License.txt) file for details.

## Credits

- **Original X-Ray Monolith**: [TheMrDemonized](https://github.com/themrdemonized/xray-monolith)
- **OpenXRay Team**: [OpenXRay](https://github.com/openxray)
- **Intel**: TBB Library
- **LuaJIT**: Scripting engine
- **True PIP**: [m22specner](https://github.com/m22spencer/xray-monolith)
