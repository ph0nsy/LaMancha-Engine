#pragma once

#include "iniReader.h"
#include "buildConfigVars.h"
#include "engineConfigVars.h"
#include "generalConfigVars.h"

namespace LaMancha {
  namespace Config {
    static const char* subsectionSuffix(const char* _name, const char* _prefix)
    {
      while (*_prefix && *_name == *_prefix)
      {
        ++_name;
        ++_prefix;
      }
      return (*_prefix == '\0') ? _name : nullptr;
    }

    void loadBuildFromINI(Build::BuildConfig& _build)
    {
      Utils::IniReader r;
      ResultVoid result = r.load("build.ini");
      if (!result)
      {
        Logging::ErrorLog("[Config] Failed to load build.ini: %s\n", result.message);
        return;
      }

      // [general]
      _build.projectName = r.getString("general", "project_name", "Unknown");
      _build.projectVersion = r.getString("general", "project_version", "v 0.0.1");

      // [target]
      _build.platform = static_cast<Build::Target>(r.getInt("target", "platform", 0));
      _build.graphicsBackend = static_cast<Build::GraphicBEnd>(r.getInt("target", "graphics_backend", 0));
      _build.audioBackend = static_cast<Build::AudioBEnd>(r.getInt("target", "graphics_backend", 0));

      // [directories]
      _build.includeDirCount = r.getStringList("directories", "include_dirs", _build.includeDirs, 8u);
      _build.excludeDirCount = r.getStringList("directories", "exclude_dirs", _build.excludeDirs, 8u);

      // [packaging]
      _build.compressAssets = r.getBool("packaging", "compress_assets", true);
      _build.createPak = r.getBool("packaging", "create_pak", true);
      _build.pakFilename = r.getString("packaging", "pak_filename", "gameAssets.pak");
    }

    struct EngineConfig
    {
      const char* projectRoot = ""; ///< Relative path from inside engineSource folder
      WindowConfig window;
      Engine::MemoryConfig memory;
      Engine::AudioConfig audio;
      Engine::LoggingConfig logging;
    };

    void loadEngineFromINI(EngineConfig& _eg)
    {
      Utils::IniReader r;
      ResultVoid result = r.load("engine.ini");
      if (!result)
      {
        Logging::ErrorLog("[Config] Failed to load engine.ini: %s\n", result.message);
        return;
      }

      // [project]
      _eg.projectRoot = r.getString("project", "root", "../LaManchaProjects/ExampleProject");
      _eg.window.title = r.getString("project", "window_title", "LaMancha Window");

      // [graphics]
      _eg.window.width = r.getInt("graphics", "default_resolution_width", 1920);
      _eg.window.height = r.getInt("graphics", "default_resolution_height", 1080);
      _eg.window.fullscreen = r.getBool("graphics", "fullscreen", true);
      _eg.window.vsync = r.getBool("graphics", "vsync", true);
      _eg.window.targetFps = r.getInt("graphics", "target_fps", 60);

      // [audio] flat keys on the parent section
      _eg.audio.fallback = r.getBool("audio", "fallback", true);
      _eg.audio.masterVolume = r.getFloat("audio", "master_volume", 0.8f);
      _eg.audio.sampleRate = r.getInt("audio", "sample_rate", 44100);
      _eg.audio.bufferSize = r.getInt("audio", "buffer_size", 2048);

      // [audio.*] subsection array
      // iterateSections visits [audio.windows], [audio.linux], [audio.r36s]
      // in the order they appear in the file.
      r.iterateSections("audio.", [&](const Utils::IniSection& sec)
        {
          OSAudioConfig cfg;
          const char* osName = subsectionSuffix(sec.name, "audio."); // Key on Hash map

          const char* channels = r.getString(sec, "channels", "stereo");
          cfg.stereo = Utils::strEqI(channels, "stereo");

          cfg.frequency = r.getInt(sec, "frequency", 44100);
          cfg.hrtf = r.getBool(sec, "hrtf", false);
          cfg.device = r.getString(sec, "device", "");

          if (osName == "windows") { _eg.audio.osConfig.set(Build::Target::Windows, cfg); }
          else if (osName == "r36s") { _eg.audio.osConfig.set(Build::Target::R36S, cfg); }
          else if (osName == "mac") { _eg.audio.osConfig.set(Build::Target::Mac, cfg); }
          else if (osName == "ios") { _eg.audio.osConfig.set(Build::Target::iOS, cfg); }
          else { _eg.audio.osConfig.set(Build::Target::Linux, cfg); }
        });

      // [memory]
      _eg.memory.texturePoolMb = r.getInt("memory", "texture_pool_mb", 64);
      _eg.memory.audioPoolMb = r.getInt("memory", "audio_pool_mb", 32);
      _eg.memory.scriptPoolMb = r.getInt("memory", "script_pool_mb", 8);

      // [logging]
      _eg.logging.toScreen = r.getBool("logging", "log_to_screen", true);
      _eg.logging.toConsole = r.getBool("logging", "log_to_console", true);
      _eg.logging.toFile = r.getBool("logging", "log_to_file", false);
      _eg.logging.logFile = r.getString("logging", "log_file", "game.log");

#if LAMANCHA_DEBUG
      r.debugDump();
      r.debugPoolUsage();
#endif
    }

    static void loadConfigFromINI(EngineConfig& egCfg/*, Build::BuildConfig& bdCfg */)
    {
      /* loadBuildFromINI(bdCfg); */
      loadEngineFromINI(egCfg);
    }
  }
};
