<!--
---
title: "LaMancha Engine"
description: "A lightweight, modular, cross-platform 2D game engine written in C++17 with optional Lua scripting, optimized for low-end devices like the R36S and modern desktop systems."
tags: ["game engine", "C++", "Lua", "OpenGL ES", "ECS", "cross-platform", "2D engine", "low-end hardware", "R36S", "R36XX"]
author: "ph0nsy"
og_image: "./promo/LaManchaEngine_Logo.svg"
og_title: "LaMancha Engine — Lightweight C++17 2D Game Engine"
og_description: "A fast, modular, cross-platform 2D engine built in C++17 with Lua scripting, optimized for handheld and desktop devices."
canonical_url: "https://github.com/ph0nsy/LaMancha-Engine"
license: "MIT"
version: "0.0.1"
--- 
-->

> ⚠️ **Note:** This project is currently under active development ⚠️
>
> The content below represents an early layout and may evolve in future versions. The content may be outdated or include sections that have not been implemented yet.

<br>

<div align=center>
	<img alt="LaMancha_Logo" src="./promo/LaManchaEngine_Logo.svg#svgView(viewBox(35, 480, 1985, 970))" width=360 height=160/>
</div>

<br><br>

> LaMancha Engine is a lightweight, open-source 2D game engine written in C++17 with Lua scripting, featuring an ECS architecture, OpenGL ES rendering, and cross-platform compatibility for Linux, Windows, and low-end handheld devices (**R36S**). It allows indie developers to create fast, portable 2D games with minimal dependencies and full cross-platform support.

## Overview

<p>
  <img src="https://img.shields.io/badge/STL--Free_ECS-blue?style=for-the-badge">
  <img src="https://img.shields.io/badge/Async_Threadpool-orange?style=for-the-badge">
  <img src="https://img.shields.io/badge/Lightweight-slategray?style=for-the-badge">
  <img src="https://img.shields.io/badge/2D-OpenGL_ES_2.0+-darkgreen?style=for-the-badge">
  <img src="https://img.shields.io/badge/Lua_Scripting-purple?style=for-the-badge&logo=lua&logoColor=white">
  <img src="https://img.shields.io/badge/C++-%2300599C.svg?style=for-the-badge&logo=c%2B%2B&logoColor=white">
</p>

<p>
	<img alt="Linux" src="https://img.shields.io/badge/Linux-FCC624?style=for-the-badge&logo=linux&logoColor=black">
	<img alt="Windows" src="https://img.shields.io/badge/Windows-0078D6.svg?style=for-the-badge&logo=data:image/png%2bxml;base64,iVBORw0KGgoAAAANSUhEUgAAAgAAAAIACAQAAABecRxxAAAABGdBTUEAALGPC/xhBQAAACBjSFJNAAB6JgAAgIQAAPoAAACA6AAAdTAAAOpgAAA6mAAAF3CculE8AAAAAmJLR0QA/4ePzL8AAAAHdElNRQfoBh4XDC5gTxF1AAAEt0lEQVR42u3dsQ2DMBCG0TNiJXaApSiyR+YwO6TMDJnjWIAC0lg6vddf88v6WrfMoLqjbc+PssdquuomE4AAAAIACAAgAIAAAAIACAAgAIAAAAIACAAgAIAAAAIACAAgAIAAAAIACAAgAIAAAAIACAAgAIAAAAIACAAgAIAAAAIACAAgAIAAAAIACAAgACAAgAAAAgAIACAAgAAAAgAIACAAgAAAAgAIACAAgAAAAgAIACAAgAAAAgAIACAAgAAAAgAIACAAgAAAAgAIACAAgAAAAgAIACAAgAAAAgAIACAAIACAAAACAAgAIACAAAACAAgAIACAAAACAAgAIACAAAACAAgAIACAAAACAAgAIACAAAACAAgAIACAAAACAAgAIACAAAACAAgAIACAAAACAAgAIAAgAIAAAAIACAAgAIAAAAIACAAgAIAAAAIACAAgAIAAAAIACAAgAIAAAAIACAAgAIAAAAIACAAgAIAAAAIACAAgAIAAAAIACAAgAIAAAAIACAAgACAAgAAAAgAIACAAgAAAAgAIACAAgAAAAgAIACAAgAAAAgAIACAAgAAAAgAIACAAgAAAAgAIACAAgAAAAgAIACAAgAAAAgAIACAAgAAAAgAIACAAIACAAAACAAgAIACAAAACAAgAIACAAAACAAgAIACAAAACAAgAIACAAAACAAgAIACAAAACAAgAIACAAAACAAgAIACAAAACAAgAIACAAAACAAgAIAAgAIAAAAIACAAgAIAAAAIACAAgAIAAAAIACAAgAIAAAAIACAAgAIAAAAIAjDHH2wjlff+6OuJnOgAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACA+1p2I5T3afvzo3zFYrrq5liNwKXF26jP78AgAIAAAAIACAAgAIAAAAIACAAgAIAAAAIACAAgAIAAAAIACAAgAIAAAAIACAAgAIAAAAIACAAgAIAAAAIACAAgAIAAAAIACAAgAIAAAAIACAAIgAlAAAABAAQAEABAAAABAAQAEABAAAABAAQAEABAAAABAAQAEABAAAABAAQAEABAAAABAAQAEABAAAABAAQAEABAAAABAAQAEABAAAABAAQAEAAQAEAAAAEABAAQAEAAAAEABAAQAEAAAAEABAAQAEAAAAEABAAQAEAAAAEABAAQAEAAAAEABAAQAEAAAAEABAAQAEAAAAEABAAQAEAAAAEABAAQABAAQAAAAQAEABAAQAAAAQAEABAAQAAAAQAEABAAQAAAAQAEABAAQAAAAQAEABAAQAAAAQAEABAAQAAAAQAEABAAQAAAAQAEABAAQAAAAQAEABAAEABAAAABAAQAEABAAAABAAQAEABAAAABAAQAEABAAAABAAQAEABAAAABAAQAEABAAAABAAQAEABAAAABAAQAEABAAAABAAQAEABAAAABAAQAEAAQAEAAAAEABAAQAEAAAAEABAAQAEAAAAEABAAQAEAAAAEABAAQAEAAAAEABAAQAEAAAAEABAAQAEAAAAEABAAQAEAAAAEABAAQAEAAAAEABAAQABAAQAAAAQAEABAAQAAAAQAEABAAQAAAAQAEABAAQAAAAQAEABAAQAAAAQAGOQHLshFV6K14LgAAACV0RVh0ZGF0ZTpjcmVhdGUAMjAyNC0wNi0zMFQyMzoxMjo0NiswMDowMOA+OTsAAAAldEVYdGRhdGU6bW9kaWZ5ADIwMjQtMDYtMzBUMjM6MTI6NDYrMDA6MDCRY4GHAAAAAElFTkSuQmCC&logoColor=white">
    <a target="_blank" href="https://github.com/ph0nsy/LaMancha-Engine/network"><img alt="version" src="https://img.shields.io/badge/version-0.0.1-blue.svg?style=for-the-badge&labelColor=888888&color=2EA967&logo=github"></a>
     <!--<a target="_blank" href="https://github.com/ph0nsy/LaMancha-Engine/stargazers"><img alt="stars" src="https://img.shields.io/github/stars/ph0nsy/LaMancha-Engine.svg?style=for-the-badge&labelColor=888888&color=2EA967&logo=github"></a>-->
     <!--<a target="_blank" href="https://github.com/cocos/LaMancha-Engine/forks"><img alt="forks" src="https://img.shields.io/github/forks/ph0nsy/LaMancha-Engine.svg?style=for-the-badge&labelColor=888888&color=2EA967&logo=github"></a>-->
    <a target="_blank" href="./LICENSE"><img alt="license" src="https://img.shields.io/badge/license-MIT-blue.svg?style=for-the-badge&labelColor=888888&color=2EA967&logo=github"></a>
</p>

**LaMancha Engine** is a lightweight, modular, cross-platform 2D engine written in <img alt="_C++ 17_" align=center src="https://img.shields.io/badge/C++_17-%2300599C.svg?logo=c%2B%2B&logoColor=white"/>, optimized for low-end devices (**R36S**) and modern Linux/Windows systems.

Key features:

- **Lightweight & fast** thanks to minimal heap allocations, `.ini`-based configuration and event-driven architecture.  
- **Event-driven input & gameplay** prevents constant polling.  
- **Cross-platform rendering** via OpenGL ES 2.0+ via GLAD and GLM.  
- **Asynchronous tasks** with threadpool handling asset loading, logging, and profiling.  
- **Custom STL-free [ECS](## "Entity-Component System")**.  
- **<img alt=_Lua_ align=center src='https://img.shields.io/badge/Lua-2C2D72?logo=lua&logoColor=white'/> scripting**.  

## Getting Started

**Clone the repository:**

```bash
git clone https://github.com/ph0nsy/LaMancha-Engine.git
cd LaMancha-Engine
```

**Build the engine (Makefile-based) by running:**

| **Pure <img alt="_C++_" align=center src="https://img.shields.io/badge/C++-%2300599C.svg?logo=c%2B%2B&logoColor=white"/>** | **<img alt="_C++_" align=center src="https://img.shields.io/badge/C++-%2300599C.svg?logo=c%2B%2B&logoColor=white"/> & <img alt=_Lua_ align=center src='https://img.shields.io/badge/Lua-2C2D72?logo=lua&logoColor=white'/>** |
|:--------------:|:----------------:|
| `make`         | `make USE_LUA=1` |

**Test installation by running a demo project:**

```bash
./Builds/Demo
```

**You can configure projects using `/Tools/ProjectConfigurator` or directly modifying `project.ini` and `input.ini` in `/Game/Config/`**

You can find further steps [here](https://github.com/ph0nsy/LaMancha-Engine/wiki/Getting-Started).

## Project Layout & References

```
LaMancha-Engine/
├── 📂 EngineCore/		→ Core subsystems (ECS, Renderer, Input, Assets, Profiler, optional Lua)
├── 📂 Game/			→ Game logic, scripts, assets, configs
├── 📂 ThirdParty/		→ Static libraries (spdlog, GLAD, GLM, stb, threadpool, tinyfiledialogs, Lua, etc.)
├── 📂 Tools/			→ ProjectCreator, ProjectConfigurator, ProjectPackager, ProfilerViewer, AssetPacker
├── 📂 Builds/			→ Output folder for builds
└── main.cpp			→ Engine entry point
````

- 🧠 [API Reference](https://github.com/ph0nsy/LaMancha-Engine/wiki/API)
- ⚙️ [Engine Architecture & Thread Model](https://github.com/ph0nsy/LaMancha-Engine/wiki/Engine-Architecture-&-Design)
- 🔧 [Engine Lifecycle](https://github.com/ph0nsy/LaMancha-Engine/wiki/Engine-Lifecycle)
- 📊 [Performance Benchmarks](https://github.com/ph0nsy/LaMancha-Engine/wiki/Performance)
- 💻 [Target Hardware (R36S)](https://github.com/ph0nsy/LaMancha-Engine/wiki/Target-Hardware)
- 📚 [Full Documentation](https://github.com/ph0nsy/LaMancha-Engine/wiki)

## FAQ

**Q: Can I create a game without touching engine code?**

**A:** Yes, engine provides mechanics, not rules — all gameplay logic is added in `/Game/Systems/` (<img alt="_C++_" align=center src="https://img.shields.io/badge/C++-%2300599C.svg?logo=c%2B%2B&logoColor=white"/>) or `/Game/Scripts/` (<img alt="_Lua_" align=center src="https://img.shields.io/badge/Lua-2C2D72?logo=lua&logoColor=white"/>).

**Q: How lightweight is it?**

A: Event-driven architecture reduces CPU overhead and simplifies game logic. INI-based configuration keeps memory footprint low. Typical runtime footprint: ~30–50 MB on **R36S**; engine startup <1 second cold boot, <300ms warm boot.

**Q: Is <img alt=_Lua_ align=center src='https://img.shields.io/badge/Lua-2C2D72?logo=lua&logoColor=white'/> mandatory?**

**A:** No — <img alt=_Lua_ align=center src='https://img.shields.io/badge/Lua-2C2D72?logo=lua&logoColor=white'/> is optional. You can run the engine and games entirely in <img alt="_C++_" align=center src="https://img.shields.io/badge/C++-%2300599C.svg?logo=c%2B%2B&logoColor=white"/>'s [ECS](## "Entity-Component System") and systems.

### Licenses

Resources created for the LaMancha Engine can be used under the following licenses:

|  Resources  |    License   |
|:------------|:------------:|
|    Code     |      MIT     |
|    Art      | CC BY-SA 4.0 |
|    Audio    | CC BY-ND 4.0 |

## Donate

<!-- Update with Ko-Fi link -->
