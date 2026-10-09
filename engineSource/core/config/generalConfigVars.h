/*
 *  LaMancha Engine
 *  File: generalConfigVars.h
 *
 *  Copyright (C) 2026 Alonso Moreno
 *
 */

namespace LaMancha {
  namespace Config
  {
    struct WindowConfig {
      const char* title;
      i32  width = 1920;
      i32  height = 1080;
      bool fullscreen = true;
      bool vsync = true;
      i32  targetFps = 60;
    };

    struct OSAudioConfig {
      i32 frequency = 44100;
      bool stereo = true;
      bool hrtf = false;
      const char* device = nullptr;
      const char* deviceFbk = nullptr;
    };
  }
}
