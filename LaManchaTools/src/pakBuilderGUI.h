#pragma once
#include <imgui.h>
#include <tinyfiledialogs.h>
#include <string>
#include <vector>
#include <filesystem>
#include <thread>
#include <atomic>
#include <mutex>
#include <fstream>

namespace fs = std::filesystem;

class PakBuilderUI {
private:
    // Project selection
    char project_path[512] = "";
    bool project_loaded = false;
    
    // Asset discovery
    struct AssetFile {
        std::string relative_path;
        size_t size_bytes;
        bool selected;
        bool is_cooked;  // Already in pak cache
        std::string file_type;  // texture, audio, script, etc.
    };
    
    std::vector<AssetFile> discovered_assets;
    
    // Pak builder state
    char output_pak_path[512] = "game.pak";
    bool compress_assets = true;
    int compression_format = 0;  // 0=zlib, 1=lz4
    
    // Build cache
    struct BuildCache {
        std::string cache_file_path;
        std::map<std::string, uint32_t> file_hashes;  // path -> CRC32
    };
    BuildCache build_cache;
    
    // Progress tracking
    std::atomic<bool> is_building{false};
    std::atomic<float> build_progress{0.0f};
    std::atomic<int> current_file_index{0};
    std::atomic<int> total_files{0};
    std::string current_file_name;
    std::mutex progress_mutex;
    std::thread build_thread;
    
    // Statistics
    struct BuildStats {
        size_t total_files = 0;
        size_t files_cooked = 0;
        size_t files_cached = 0;
        size_t uncompressed_size = 0;
        size_t compressed_size = 0;
        float compression_ratio = 0.0f;
        double build_time_seconds = 0.0;
    } build_stats;
    
    // Status
    std::string status_message = "";
    bool build_successful = false;
    
public:
    ~PakBuilderUI() {
        // Wait for build thread to finish
        if (build_thread.joinable()) {
            build_thread.join();
        }
    }
    
    void Render() {
        // Project Selection
        ImGui::SeparatorText("Project Selection");
        
        ImGui::InputText("Project Path", project_path, sizeof(project_path));
        ImGui::SameLine();
        
        if (ImGui::Button("Browse...")) {
            const char* selected = tinyfd_selectFolderDialog(
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
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                             "No project loaded. Select a project folder above.");
            return;
        }
        
        ImGui::Separator();
        
        // Asset List
        ImGui::SeparatorText("Assets to Pack");
        
        // Filter controls
        static bool show_textures = true;
        static bool show_audio = true;
        static bool show_scripts = true;
        static bool show_other = true;
        
        ImGui::Checkbox("Textures", &show_textures); ImGui::SameLine();
        ImGui::Checkbox("Audio", &show_audio); ImGui::SameLine();
        ImGui::Checkbox("Scripts", &show_scripts); ImGui::SameLine();
        ImGui::Checkbox("Other", &show_other);
        
        ImGui::SameLine();
        if (ImGui::Button("Select All")) {
            for (auto& asset : discovered_assets) {
                asset.selected = true;
            }
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Deselect All")) {
            for (auto& asset : discovered_assets) {
                asset.selected = false;
            }
        }
        
        // Asset table
        if (ImGui::BeginTable("AssetTable", 5, 
            ImGuiTableFlags_Borders | 
            ImGuiTableFlags_RowBg | 
            ImGuiTableFlags_ScrollY,
            ImVec2(0, 300))) {
            
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 30);
            ImGui::TableSetupColumn("File", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableHeadersRow();
            
            int asset_idx = 0;
            for (auto& asset : discovered_assets) {
                // Apply filters
                if (asset.file_type == "texture" && !show_textures) continue;
                if (asset.file_type == "audio" && !show_audio) continue;
                if (asset.file_type == "script" && !show_scripts) continue;
                if (asset.file_type == "other" && !show_other) continue;
                
                ImGui::TableNextRow();
                
                // Checkbox
                ImGui::TableSetColumnIndex(0);
                ImGui::PushID(asset_idx++);
                ImGui::Checkbox("##select", &asset.selected);
                ImGui::PopID();
                
                // Filename
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%s", asset.relative_path.c_str());
                
                // Type
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%s", asset.file_type.c_str());
                
                // Size
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%s", FormatFileSize(asset.size_bytes).c_str());
                
                // Status
                ImGui::TableSetColumnIndex(4);
                if (asset.is_cooked) {
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Cached");
                } else {
                    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "New");
                }
            }
            
            ImGui::EndTable();
        }
        
        ImGui::Separator();
        
        // Build Settings
        ImGui::SeparatorText("Build Settings");
        
        ImGui::InputText("Output PAK", output_pak_path, sizeof(output_pak_path));
        ImGui::SameLine();
        
        if (ImGui::Button("...")) {
            const char* selected = tinyfd_saveFileDialog(
                "Save PAK File",
                "game.pak",
                1,
                (const char*[]){"*.pak"},
                "PAK Files"
            );
            
            if (selected) {
                strncpy(output_pak_path, selected, sizeof(output_pak_path) - 1);
            }
        }
        
        ImGui::Checkbox("Compress Assets", &compress_assets);
        
        if (compress_assets) {
            const char* formats[] = { "ZLIB (better compression)", "LZ4 (faster decompression)" };
            ImGui::Combo("Compression", &compression_format, formats, 2);
        }
        
        ImGui::Separator();
        
        // Build Button
        if (!is_building) {
            if (ImGui::Button("Build PAK", ImVec2(200, 50))) {
                StartBuild();
            }
            
            ImGui::SameLine();
            if (ImGui::Button("Build (Force Rebuild)", ImVec2(200, 50))) {
                StartBuild(true);  // Force rebuild
            }
        } else {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Building...");
        }
        
        // Progress Bar
        if (is_building || build_progress > 0.0f) {
            ImGui::Separator();
            ImGui::SeparatorText("Build Progress");
            
            {
                std::lock_guard<std::mutex> lock(progress_mutex);
                
                ImGui::ProgressBar(build_progress, ImVec2(-1, 0), 
                                  std::to_string((int)(build_progress * 100)).append("%").c_str());
                
                ImGui::Text("File %d / %d: %s", 
                           current_file_index.load(), 
                           total_files.load(),
                           current_file_name.c_str());
            }
        }
        
        // Build Statistics
        if (!is_building && build_stats.total_files > 0) {
            ImGui::Separator();
            ImGui::SeparatorText("Build Statistics");
            
            ImGui::Text("Total Files: %zu", build_stats.total_files);
            ImGui::Text("Files Cooked: %zu", build_stats.files_cooked);
            ImGui::Text("Files Cached: %zu", build_stats.files_cached);
            ImGui::Text("Uncompressed Size: %s", FormatFileSize(build_stats.uncompressed_size).c_str());
            ImGui::Text("Compressed Size: %s", FormatFileSize(build_stats.compressed_size).c_str());
            ImGui::Text("Compression Ratio: %.1f%%", build_stats.compression_ratio);
            ImGui::Text("Build Time: %.2fs", build_stats.build_time_seconds);
        }
        
        // Status Message
        if (!status_message.empty()) {
            ImGui::Separator();
            if (build_successful) {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%s", status_message.c_str());
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%s", status_message.c_str());
            }
        }
    }
    
private:
    void LoadProject() {
        fs::path proj_path(project_path);
        
        if (!fs::exists(proj_path)) {
            status_message = "Error: Project path does not exist";
            project_loaded = false;
            return;
        }
        
        // Read build.ini to find asset directories
        fs::path build_ini = proj_path / "config" / "build.ini";
        if (!fs::exists(build_ini)) {
            status_message = "Error: build.ini not found in config/";
            project_loaded = false;
            return;
        }
        
        // Parse build.ini to get asset directories
        std::string asset_dirs = "assets";  // Default
        // TODO: Parse build.ini properly to get include_assets
        
        // Discover assets
        DiscoverAssets(proj_path, asset_dirs);
        
        // Load build cache
        LoadBuildCache(proj_path);
        
        // Set default output path
        fs::path default_output = proj_path / "build" / "game.pak";
        strncpy(output_pak_path, default_output.string().c_str(), sizeof(output_pak_path) - 1);
        
        project_loaded = true;
        status_message = "Project loaded. Found " + std::to_string(discovered_assets.size()) + " assets.";
    }
    
    void DiscoverAssets(const fs::path& proj_path, const std::string& asset_dirs) {
        discovered_assets.clear();
        
        // For now, just scan assets/ folder
        fs::path assets_path = proj_path / "assets";
        
        if (!fs::exists(assets_path)) {
            return;
        }
        
        for (const auto& entry : fs::recursive_directory_iterator(assets_path)) {
            if (entry.is_regular_file()) {
                AssetFile asset;
                asset.relative_path = fs::relative(entry.path(), assets_path).string();
                asset.size_bytes = fs::file_size(entry.path());
                asset.selected = true;
                
                // Determine file type
                std::string ext = entry.path().extension().string();
                if (ext == ".png" || ext == ".jpg" || ext == ".bmp") {
                    asset.file_type = "texture";
                } else if (ext == ".ogg" || ext == ".wav" || ext == ".mp3") {
                    asset.file_type = "audio";
                } else if (ext == ".lua" || ext == ".js") {
                    asset.file_type = "script";
                } else {
                    asset.file_type = "other";
                }
                
                // Check if already cooked
                asset.is_cooked = IsAssetCached(asset.relative_path);
                
                discovered_assets.push_back(asset);
            }
        }
    }
    
    void LoadBuildCache(const fs::path& proj_path) {
        build_cache.cache_file_path = (proj_path / "build" / ".pak_cache").string();
        build_cache.file_hashes.clear();
        
        if (!fs::exists(build_cache.cache_file_path)) {
            return;
        }
        
        // Load cache file
        std::ifstream cache_file(build_cache.cache_file_path);
        std::string line;
        
        while (std::getline(cache_file, line)) {
            // Format: "path,crc32"
            size_t comma_pos = line.find(',');
            if (comma_pos != std::string::npos) {
                std::string path = line.substr(0, comma_pos);
                uint32_t crc = std::stoul(line.substr(comma_pos + 1));
                build_cache.file_hashes[path] = crc;
            }
        }
    }
    
    bool IsAssetCached(const std::string& relative_path) {
        return build_cache.file_hashes.find(relative_path) != build_cache.file_hashes.end();
    }
    
    void StartBuild(bool force_rebuild = false) {
        // Join previous thread if exists
        if (build_thread.joinable()) {
            build_thread.join();
        }
        
        // Reset progress
        build_progress = 0.0f;
        current_file_index = 0;
        is_building = true;
        build_successful = false;
        status_message = "";
        
        // Reset stats
        build_stats = BuildStats();
        
        // Start build thread
        build_thread = std::thread([this, force_rebuild]() {
            BuildPakFile(force_rebuild);
        });
    }
    
    void BuildPakFile(bool force_rebuild) {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        try {
            // Count selected files
            std::vector<AssetFile*> files_to_pack;
            for (auto& asset : discovered_assets) {
                if (asset.selected) {
                    // Skip cached files unless force rebuild
                    if (!force_rebuild && asset.is_cooked) {
                        build_stats.files_cached++;
                        continue;
                    }
                    files_to_pack.push_back(&asset);
                }
            }
            
            total_files = files_to_pack.size();
            build_stats.total_files = total_files;
            
            // TODO: Actually build PAK file using pak_writer
            // For now, simulate the build process
            
            for (size_t i = 0; i < files_to_pack.size(); i++) {
                AssetFile* asset = files_to_pack[i];
                
                // Update progress
                {
                    std::lock_guard<std::mutex> lock(progress_mutex);
                    current_file_index = i + 1;
                    current_file_name = asset->relative_path;
                    build_progress = (float)(i + 1) / total_files;
                }
                
                // Simulate processing time
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                
                // Update stats
                build_stats.uncompressed_size += asset->size_bytes;
                build_stats.compressed_size += asset->size_bytes * 0.6;  // Simulated compression
                build_stats.files_cooked++;
            }
            
            // Calculate compression ratio
            if (build_stats.uncompressed_size > 0) {
                build_stats.compression_ratio = 
                    (1.0f - (float)build_stats.compressed_size / build_stats.uncompressed_size) * 100.0f;
            }
            
            auto end_time = std::chrono::high_resolution_clock::now();
            build_stats.build_time_seconds = 
                std::chrono::duration<double>(end_time - start_time).count();
            
            build_successful = true;
            status_message = "PAK file built successfully!";
            
        } catch (const std::exception& e) {
            build_successful = false;
            status_message = std::string("Build failed: ") + e.what();
        }
        
        is_building = false;
        build_progress = 1.0f;
    }
    
    std::string FormatFileSize(size_t bytes) {
        const char* units[] = { "B", "KB", "MB", "GB" };
        int unit_idx = 0;
        double size = bytes;
        
        while (size >= 1024.0 && unit_idx < 3) {
            size /= 1024.0;
            unit_idx++;
        }
        
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "%.1f %s", size, units[unit_idx]);
        return std::string(buffer);
    }
};