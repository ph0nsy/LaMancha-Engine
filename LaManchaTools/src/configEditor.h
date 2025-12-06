#pragma once
#include <imgui.h>
#include <tinyfiledialogs.h>
#include <string>
#include <filesystem>
#include "inih/ini.h"

namespace fs = std::filesystem;

class ConfigEditor {
  private:
    // Project selection
    char project_path[512] = "";
  bool project_loaded = false;

  // Configuration data (loaded from INIs)
  struct BuildConfig {
    char include_dirs[512] = "src";
    char exclude_dirs[512] = "";
    char include_assets[512] = "assets";
    char exclude_assets[512] = "assets/*.psd";
    char platform[64] = "windows";
    char graphics_backend[64] = "glfw";
    bool compress_assets = true;
    char compression_format[32] = "zlib";
    bool create_pak = false;
    char pak_filename[128] = "game.pak";
  }
  build_config;

  struct GameConfig { // Previously named engine.ini, now game.ini
    int resolution_width = 1280;
    int resolution_height = 720;
    bool fullscreen = false;
    bool vsync = true;
    int target_fps = 60;
    float master_volume = 0.8 f;
    int sample_rate = 44100;
    int buffer_size = 2048;
    int texture_pool_mb = 64;
    int audio_pool_mb = 32;
    int script_pool_mb = 8;
    char log_level[32] = "info";
    bool log_to_file = true;
    char log_file[128] = "game.log";
  }
  game_config;

  // UI state
  bool config_modified = false;
  std::string status_message = "";

  public: void Render() {
    // Project Selection Section
    ImGui::SeparatorText("Project Selection");

    ImGui::InputText("Project Path", project_path, sizeof(project_path));
    ImGui::SameLine();

    if (ImGui::Button("Browse...")) {
      const char * selected = tinyfd_selectFolderDialog(
        "Select Project Folder",
        ""
      );

      if (selected) {
        strncpy(project_path, selected, sizeof(project_path) - 1);
      }
    }

    ImGui::SameLine();

    if (ImGui::Button("Load Project")) {
      LoadProject();
    }

    if (!project_loaded) {
      ImGui::TextColored(ImVec4(1.0 f, 0.5 f, 0.0 f, 1.0 f),
        "No project loaded. Select a project folder above.");
      return;
    }

    ImGui::Separator();

    // Configuration Tabs
    if (ImGui::BeginTabBar("ConfigTabs")) {
      if (ImGui::BeginTabItem("Build Settings")) {
        RenderBuildConfig();
        ImGui::EndTabItem();
      }

      if (ImGui::BeginTabItem("Game Settings")) {
        RenderGameConfig();
        ImGui::EndTabItem();
      }

      if (ImGui::BeginTabItem("Input Mapping")) {
        RenderInputConfig();
        ImGui::EndTabItem();
      }

      ImGui::EndTabBar();
    }

    ImGui::Separator();

    // Save Button
    if (config_modified) {
      ImGui::TextColored(ImVec4(1.0 f, 1.0 f, 0.0 f, 1.0 f),
        "* Configuration modified");
    }

    if (ImGui::Button("Save All Changes", ImVec2(200, 40))) {
      SaveAllConfigs();
    }

    // Status message
    if (!status_message.empty()) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(0.0 f, 1.0 f, 0.0 f, 1.0 f),
        "%s", status_message.c_str());
    }
  }

  private: void LoadProject() {
    fs::path proj_path(project_path);

    // Validate project structure
    if (!fs::exists(proj_path / "config")) {
      status_message = "Error: No config/ folder found";
      project_loaded = false;
      return;
    }

    // Load build.ini
    LoadBuildConfig(proj_path / "config" / "build.ini");

    // Load game.ini (renamed from engine.ini)
    LoadGameConfig(proj_path / "config" / "game.ini");

    project_loaded = true;
    config_modified = false;
    status_message = "Project loaded successfully";
  }

  void LoadBuildConfig(const fs::path & ini_path) {
    if (!fs::exists(ini_path)) return;

    ini_parse(ini_path.string().c_str(),
      [](void * user,
        const char * section,
          const char * name,
            const char * value) {
        auto * config = static_cast < BuildConfig * > (user);
        std::string sec(section), key(name), val(value);

        if (sec == "directories") {
          if (key == "include_dirs") strncpy(config -> include_dirs, val.c_str(), 511);
          else if (key == "exclude_dirs") strncpy(config -> exclude_dirs, val.c_str(), 511);
          else if (key == "include_assets") strncpy(config -> include_assets, val.c_str(), 511);
          else if (key == "exclude_assets") strncpy(config -> exclude_assets, val.c_str(), 511);
        } else if (sec == "target") {
          if (key == "platform") strncpy(config -> platform, val.c_str(), 63);
          else if (key == "graphics_backend") strncpy(config -> graphics_backend, val.c_str(), 63);
        } else if (sec == "packaging") {
          if (key == "compress_assets") config -> compress_assets = (val == "true");
          else if (key == "compression_format") strncpy(config -> compression_format, val.c_str(), 31);
          else if (key == "create_pak") config -> create_pak = (val == "true");
          else if (key == "pak_filename") strncpy(config -> pak_filename, val.c_str(), 127);
        }

        return 1;
      }, &
      build_config);
  }

  void LoadGameConfig(const fs::path & ini_path) {
    if (!fs::exists(ini_path)) return;

    ini_parse(ini_path.string().c_str(),
      [](void * user,
        const char * section,
          const char * name,
            const char * value) {
        auto * config = static_cast < GameConfig * > (user);
        std::string sec(section), key(name), val(value);

        if (sec == "graphics") {
          if (key == "resolution_width") config -> resolution_width = std::stoi(val);
          else if (key == "resolution_height") config -> resolution_height = std::stoi(val);
          else if (key == "fullscreen") config -> fullscreen = (val == "true");
          else if (key == "vsync") config -> vsync = (val == "true");
          else if (key == "target_fps") config -> target_fps = std::stoi(val);
        } else if (sec == "audio") {
          if (key == "master_volume") config -> master_volume = std::stof(val);
          else if (key == "sample_rate") config -> sample_rate = std::stoi(val);
          else if (key == "buffer_size") config -> buffer_size = std::stoi(val);
        } else if (sec == "memory") {
          if (key == "texture_pool_mb") config -> texture_pool_mb = std::stoi(val);
          else if (key == "audio_pool_mb") config -> audio_pool_mb = std::stoi(val);
          else if (key == "script_pool_mb") config -> script_pool_mb = std::stoi(val);
        } else if (sec == "logging") {
          if (key == "log_level") strncpy(config -> log_level, val.c_str(), 31);
          else if (key == "log_to_file") config -> log_to_file = (val == "true");
          else if (key == "log_file") strncpy(config -> log_file, val.c_str(), 127);
        }
        return 1;
      }, &
      game_config);
  }

  void RenderBuildConfig() {
    ImGui::Text("Build Configuration (build.ini)");
    ImGui::Spacing();

    // Directories
    ImGui::SeparatorText("Source Directories");
    if (ImGui::InputText("Include Dirs", build_config.include_dirs, 512)) {
      config_modified = true;
    }
    ImGui::TextWrapped("Comma-separated list (e.g., src, src/game, src/entities)");

    if (ImGui::InputText("Exclude Dirs", build_config.exclude_dirs, 512)) {
      config_modified = true;
    }
    ImGui::TextWrapped("Comma-separated list (e.g., src/editor, src/tests)");

    ImGui::Spacing();

    // Assets
    ImGui::SeparatorText("Asset Directories");
    if (ImGui::InputText("Include Assets", build_config.include_assets, 512)) {
      config_modified = true;
    }

    if (ImGui::InputText("Exclude Assets", build_config.exclude_assets, 512)) {
      config_modified = true;
    }
    ImGui::TextWrapped("Patterns like: assets/*.psd, assets/source");

    ImGui::Spacing();

    // Target Platform
    ImGui::SeparatorText("Target Configuration");

    const char * platforms[] = {
      "windows",
      "linux",
      "macos",
      "r36s"
    };
    int platform_idx = 0;
    for (int i = 0; i < 4; i++) {
      if (strcmp(build_config.platform, platforms[i]) == 0) {
        platform_idx = i;
        break;
      }
    }

    if (ImGui::Combo("Platform", & platform_idx, platforms, 4)) {
      strncpy(build_config.platform, platforms[platform_idx], 63);

      // Auto-adjust graphics backend
      if (strcmp(build_config.platform, "r36s") == 0) {
        strncpy(build_config.graphics_backend, "egl", 63);
      } else {
        strncpy(build_config.graphics_backend, "glfw", 63);
      }

      config_modified = true;
    }

    const char * backends[] = {
      "glfw",
      "egl"
    };
    int backend_idx = (strcmp(build_config.graphics_backend, "egl") == 0) ? 1 : 0;

    if (ImGui::Combo("Graphics Backend", & backend_idx, backends, 2)) {
      strncpy(build_config.graphics_backend, backends[backend_idx], 63);
      config_modified = true;
    }

    ImGui::Spacing();

    // Packaging
    ImGui::SeparatorText("Asset Packaging");

    if (ImGui::Checkbox("Compress Assets", & build_config.compress_assets)) {
      config_modified = true;
    }

    const char * formats[] = {
      "zlib",
      "lz4"
    };
    int format_idx = (strcmp(build_config.compression_format, "lz4") == 0) ? 1 : 0;

    if (ImGui::Combo("Compression", & format_idx, formats, 2)) {
      strncpy(build_config.compression_format, formats[format_idx], 31);
      config_modified = true;
    }

    if (ImGui::Checkbox("Create PAK File", & build_config.create_pak)) {
      config_modified = true;
    }

    if (build_config.create_pak) {
      if (ImGui::InputText("PAK Filename", build_config.pak_filename, 128)) {
        config_modified = true;
      }
    }
  }

  void RenderGameConfig() {
    ImGui::Text("Game Runtime Configuration (game.ini)");
    ImGui::Spacing();

    // Graphics
    ImGui::SeparatorText("Graphics Settings");

    if (ImGui::InputInt("Resolution Width", & game_config.resolution_width)) {
      config_modified = true;
    }

    if (ImGui::InputInt("Resolution Height", & game_config.resolution_height)) {
      config_modified = true;
    }

    // Quick resolution presets
    if (ImGui::Button("1280x720")) {
      game_config.resolution_width = 1280;
      game_config.resolution_height = 720;
      config_modified = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("1920x1080")) {
      game_config.resolution_width = 1920;
      game_config.resolution_height = 1080;
      config_modified = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("640x480 (R36S)")) {
      game_config.resolution_width = 640;
      game_config.resolution_height = 480;
      config_modified = true;
    }

    if (ImGui::Checkbox("Fullscreen", & game_config.fullscreen)) {
      config_modified = true;
    }

    if (ImGui::Checkbox("VSync", & game_config.vsync)) {
      config_modified = true;
    }

    if (ImGui::InputInt("Target FPS", & game_config.target_fps)) {
      config_modified = true;
    }

    ImGui::Spacing();

    // Audio
    ImGui::SeparatorText("Audio Settings");

    if (ImGui::SliderFloat("Master Volume", & game_config.master_volume, 0.0 f, 1.0 f)) {
      config_modified = true;
    }

    if (ImGui::InputInt("Sample Rate", & game_config.sample_rate)) {
      config_modified = true;
    }

    if (ImGui::InputInt("Buffer Size", & game_config.buffer_size)) {
      config_modified = true;
    }

    ImGui::Spacing();

    // Memory
    ImGui::SeparatorText("Memory Pools");

    if (ImGui::InputInt("Texture Pool (MB)", & game_config.texture_pool_mb)) {
      config_modified = true;
    }

    if (ImGui::InputInt("Audio Pool (MB)", & game_config.audio_pool_mb)) {
      config_modified = true;
    }

    if (ImGui::InputInt("Script Pool (MB)", & game_config.script_pool_mb)) {
      config_modified = true;
    }

    ImGui::Spacing();

    // Logging
    ImGui::SeparatorText("Logging");

    const char * log_levels[] = {
      "debug",
      "info",
      "warning",
      "error"
    };
    int log_idx = 1; // Default to info
    for (int i = 0; i < 4; i++) {
      if (strcmp(game_config.log_level, log_levels[i]) == 0) {
        log_idx = i;
        break;
      }
    }

    if (ImGui::Combo("Log Level", & log_idx, log_levels, 4)) {
      strncpy(game_config.log_level, log_levels[log_idx], 31);
      config_modified = true;
    }

    if (ImGui::Checkbox("Log to File", & game_config.log_to_file)) {
      config_modified = true;
    }

    if (game_config.log_to_file) {
      if (ImGui::InputText("Log File", game_config.log_file, 128)) {
        config_modified = true;
      }
    }
  }

  void RenderInputConfig() {
    ImGui::Text("Input Mapping (input.ini)");
    ImGui::Spacing();
    ImGui::TextWrapped("Input configuration editor - TODO: Implement key/button mapping UI");
    // TODO: Implement input mapping editor
  }

  void SaveAllConfigs() {
    fs::path proj_path(project_path);

    // Save build.ini
    SaveBuildConfig(proj_path / "config" / "build.ini");

    // Save game.ini
    SaveGameConfig(proj_path / "config" / "game.ini");

    config_modified = false;
    status_message = "All configurations saved!";
  }

  void SaveBuildConfig(const fs::path & ini_path) {
    std::ofstream file(ini_path);

    file << "[directories]\n";
    file << "include_dirs = " << build_config.include_dirs << "\n";
    file << "exclude_dirs = " << build_config.exclude_dirs << "\n";
    file << "include_assets = " << build_config.include_assets << "\n";
    file << "exclude_assets = " << build_config.exclude_assets << "\n\n";

    file << "[target]\n";
    file << "platform = " << build_config.platform << "\n";
    file << "graphics_backend = " << build_config.graphics_backend << "\n\n";

    file << "[packaging]\n";
    file << "compress_assets = " << (build_config.compress_assets ? "true" : "false") << "\n";
    file << "compression_format = " << build_config.compression_format << "\n";
    file << "create_pak = " << (build_config.create_pak ? "true" : "false") << "\n";
    file << "pak_filename = " << build_config.pak_filename << "\n";

    file.close();
  }

  void SaveGameConfig(const fs::path & ini_path) {
    std::ofstream file(ini_path);

    file << "[graphics]\n";
    file << "resolution_width = " << game_config.resolution_width << "\n";
    file << "resolution_height = " << game_config.resolution_height << "\n";
    file << "fullscreen = " << (game_config.fullscreen ? "true" : "false") << "\n";
    file << "vsync = " << (game_config.vsync ? "true" : "false") << "\n";
    file << "target_fps = " << game_config.target_fps << "\n\n";

    file << "[audio]\n";
    file << "master_volume = " << game_config.master_volume << "\n";
    file << "sample_rate = " << game_config.sample_rate << "\n";
    file << "buffer_size = " << game_config.buffer_size << "\n\n";

    file << "[memory]\n";
    file << "texture_pool_mb = " << game_config.texture_pool_mb << "\n";
    file << "audio_pool_mb = " << game_config.audio_pool_mb << "\n";
    file << "script_pool_mb = " << game_config.script_pool_mb << "\n\n";

    file << "[logging]\n";
    file << "log_level = " << game_config.log_level << "\n";
    file << "log_to_file = " << (game_config.log_to_file ? "true" : "false") << "\n";
    file << "log_file = " << game_config.log_file << "\n";

    file.close();
  }
};