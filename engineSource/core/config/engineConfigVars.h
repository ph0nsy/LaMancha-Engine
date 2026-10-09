/*
 *  LaMancha Engine
 *  File: engineConfigVars.h
 *
 *  Copyright (C) 2026 Alonso Moreno
 *
 */
#include "core/types/dataStructures/hashmap.h"

namespace LaMancha {
  namespace Config {
    namespace Engine {
      struct LoggingConfig {
        bool toScreen = true;
        bool toConsole = true;
        bool toFile = false;
        const char* logFile = "game.log";
      };

      struct AudioConfig {
        bool fallback = true;
        f32 masterVolume = 0.8f;
        i32 sampleRate = 44100;
        i32 bufferSize = 2048;

        // Per-OS configs: windows / linux / r36s
        DataStructures::HashMapLM<Build::Target, OSAudioConfig> osConfig = {};
      };

      struct MemoryConfig {
        i32 texturePoolMb = 64;
        i32 audioPoolMb = 32;
        i32 scriptPoolMb = 8;
      };
    }
  }
}
