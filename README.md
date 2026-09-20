# Crashday 2001 Self-Playing Fly-Demo - Source Code Reconstruction

A source reconstruction of the **Crashday self-playing fly-demo from 2001**

## Project status

Currently the source code replicates the game and its logic 1 to 1, but differences may still be present 

## Requirements

- **C++17**
- MSVC Platform Toolset **`v145`**
- Windows SDK **10.0**

## Build

1. Create an Empty C++ Project

2. Set platform to **Win32**

3. Set C++ standard to **C++17**

4. Add all **.cpp** and **.hpp** files and **folders** from **src/** and **include/**

5. Add include directories:
```text
include
src
```

6. Add linker dependencies
```text
ddraw.lib
dxguid.lib
user32.lib
gdi32.lib
winmm.lib
```

7. Set configuration type to **.exe**

8. Build in **Release (or debug) / x86**

## Running

The executable requires the original Crashday fly-demo assets

Place your executable in the original demo directory and run it

Full demo can be downloaded at [Crashday Hub](https://crashday-hub.online/file.php?id=14)

## Credits

Huge thanks to [St1ngLeR](https://github.com/St1ngLeR), Alkalll and Camomile for their help and support throughout this project

## Legal notice

Crashday was developed by **Moonbyte Studios**. Everything belong to their respective rights holders

This repository is an unofficial source-reconstruction and is not presented as an official Moonbyte Studios release
