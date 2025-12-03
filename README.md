# X-Ray Monolith: Next Gen

**Upgraded X-Ray Monolith Engine for S.T.A.L.K.E.R. Anomaly 1.5.3**

  

This is a highly optimized fork of the **X-Ray Monolith Engine**. It is designed specifically for heavy modpacks (like G.A.M.M.A.) that run hundereds of scripts, require maximum stability and memory availability.

## Key Performance Upgrades

This build introduces a modernized memory and threading architecture to break the 15 year old limitations of the X-Ray engine:

  * **LuaJIT GC64 Implementation**

      * **The 2GB Barrier is Broken:** Standard Anomaly crashes if the script engine uses \~2GB of RAM. This build enables 64-bit pointers in Lua, allowing the game to utilize **10GB+ of RAM** for scripts without crashing.
      * **Long Play Stability:** Virtually eliminates "Out of Memory" crashes during long sessions or level transitions in heavy modpacks.

  * **Intel TBB Integration**

      * Replaces the standard Windows memory allocator with **Intel Threading Building Blocks (TBB)**.
      * Drastically reduces memory fragmentation and micro stutters during object spawning and inventory management.

  * **Hybrid Multithreading Architecture**

      * **Parallelized:** Skeleton calculations (IK) and Sound Occlusion ray casting are moved to worker threads to improve combat FPS.
      * **Stabilized:** Critical game logic (A-Life and Bullet Managers) remains single threaded to prevent Lua deadlocks, ensuring solid stability.

  * **Process Affinity Masking**

      * **Core 0 Exclusion:** Automatically excludes CPU Core 0 on systems with more than 4 logical threads.
      * **Smoother Gameplay:** Prevents the main game thread from competing with Windows background processes, reducing micro stutters and improving frame time consistency.

-----

## Core Monolith Features

This engine includes all standard "Modded Exes" features required by modern mods:

  * **DLTX (Expanded LTX Support):**
      * Allows overriding sections without crashing.
      * Support for `@[newsection]` creation and overriding.
      * Advanced parameter manipulation (`>name = add`, `<name = remove`).
  * **DXML:** Allows Lua scripts to modify XML UI files on the fly.
  * **Shader Support:** Full compatibility with **Screen Space Shaders (SSS)**, **Beef's NVG**, and **Shader Scopes**.
  * **Fixes:** Includes fixes for animation smoothing, collision bugs, and the "1000 save" limit.
  * **Debug Tools:** Integrated LuaPanda support and ImGui extensions.

-----

## Installation

**Prerequisites:**

1.  **Windows 10/11** (1903 Update or newer).
2.  **Visual C++ Redistributables (2022):** [Download Latest All-in-One](https://www.techpowerup.com/download/visual-c-redistributable-runtime-package-all-in-one/)

**Steps:**

1.  Backup your existing `bin` folder in your Anomaly directory.
2.  Download the **Release** archive from this repository.
3.  Extract the contents (the `.exe` and `.dll` files) into your `ANOMALY/bin` folder.
      * *Note: `tbb.dll` is included in standard Anomaly, but you must ensure the new `lua51.dll` from this download is in the bin folder or the executable will not run.*
4.  **CRITICAL:** Navigate to `appdata/shaders_cache` and **delete the folder**.
      * *Failure to do this may result in a black screen upon loading a saved game.*
5.  Launch the game using the new executable.

-----

## Troubleshooting & FAQ

**Q: My RAM usage is very high (6GB - 10GB+). Is this a memory leak?**

**A:** **No.** This is intentional. The new allocator aggressively caches memory to prevent stuttering, and the GC64 Lua implementation uses more memory per object to ensure stability. Unused RAM is wasted RAM.

**Q: The screen is black but I can see the HUD.**

**A:** You did not delete your `shaders_cache`. Delete the folder in `appdata` and restart.

**Q: I am crashing with LNK1120 or "Entry Point Not Found".**

**A:** You likely have mismatched DLLs. Ensure you copied `lua51.dll` from the release zip into your `bin` folder and overwrote the old one.

-----

## Contributing

We welcome contributions to improve performance and stability\! Please follow these guidelines to ensure the engine remains stable for heavy modpacks.

### Guidelines

1.  **Fork & Clone:** Create a fork of the repository and clone it locally.
2.  **Branching:** Create a new branch for your feature or fix (`git checkout -b fix/memory-leak`).
3.  **Thread Safety (CRITICAL):**
      * **Do NOT** execute Lua callbacks inside TBB parallel loops. This causes deadlocks and crashes.
      * **Do NOT** write to global physics/sound states from worker threads without proper locking.
      * If you implement a new threaded system, you must prove it is safe from race conditions in your Pull Request description.

### Pull Request Process

  * Ensure your code compiles before submitting.
  * Verify that the game launches and runs without freezing.
  * Submit your PR with a clear description of the problem and your solution.

-----

## Build Instructions

This project is configured for a "One-Click Build" workflow in Visual Studio 2022. All dependency compilations (LuaJIT GC64) and file movements are handled automatically by the solution scripts.

### 1\. Development Environment

  * **Visual Studio 2022**
  * **Required Workloads:**
      * Desktop development with C++

### 2\. Getting the Source

```bash
git clone https://github.com/CnRJay/xray-monolith-gc64.git
cd xray-monolith-gc64
git submodule update --init --recursive
```

### 3\. Compilation

1.  Open `engine-vs2022.sln` in Visual Studio.
2.  Set the Solution Configuration to whatever build you want.
3.  Set the Solution Platform to **x64**.
4.  Right-click **Solution 'Engine'** in the explorer and select **Build Solution**.

*The build process will automatically compile LuaJIT with GC64 support, link the TBB libraries, and output the final executables.*

### 4\. Output Location

The compiled executable and pdb will be generated in:
`_build/_game/bin_dbg/`

The compiled LuaJIT library (`lua51.dll`) will be generated in:
`_build/_game/bin_dbg/x64/Release/` or `_build/_game/bin_dbg/x64/Release-AVX/` if you built in Release-AVX.

Simply copy the 2 files into your `ANOMALY/bin` directory to play.

-----

## Credits

  * **Original X-Ray Monolith and all its contributors:** [TheMrDemonized](https://github.com/themrdemonized/xray-monolith)
  * **OpenXRay Team:** [OpenXRay](https://github.com/openxray) For the foundation of the modern codebase.
  * **Intel:** For the TBB Library.
  * **LuaJIT:** For the scripting engine.

*Based on X-Ray 1.6 Engine (Call of Pripyat). Any changes are for non-commercial use only.*

------