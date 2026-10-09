/*
 *  LaMancha Engine
 *  File: buildConfigVars.h
 *
 *  Copyright (C) 2026 Alonso Moreno
 *
 */

namespace LaMancha {
  namespace Config {
    namespace Build
    {
      enum Target : u8 { Windows = 0, Linux, R36S, Mac, iOS };
      enum GraphicBEnd : u8 { GLFW = 0, /*Vulkan,*/ None };
      enum AudioBEnd : u8 { OpenAL = 0, None };

      struct BuildConfig {
        const char* projectName = "New Project";
        const char* projectVersion = "v 0.0.1";
        Target platform = Target::Windows;
        GraphicBEnd graphicsBackend = GraphicBEnd::GLFW;
        AudioBEnd audioBackend = AudioBEnd::OpenAL;

        // Comma-separated lists stored as raw arrays
        const char* includeDirs[8] = {};
        u32 includeDirCount = 0;

        const char* excludeDirs[8] = {};
        u32 excludeDirCount = 0;

        bool compressAssets = true;
        bool createPak = true;
        const char* pakFilename = "gameAssets.pak";
      };
    }
  }
}