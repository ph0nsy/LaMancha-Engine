cmake_minimum_required(VERSION 3.15)
project(LaManchaEngine VERSION 0.0.1 LANGUAGES CXX C)

# Project Settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Build Options
option(LAMANCHA_BUILD_ENGINE "Build engine library" ON)
option(LAMANCHA_BUILD_TOOLS "Build development tools" ON)
option(LAMANCHA_BUILD_PROJECTS "Build game projects" ON)

# Platform Detection
set(PLATFORM_DESKTOP ON)
set(PLATFORM_R36S OFF)
set(PLATFORM_WINDOWS ON)
set(PLATFORM_LINUX OFF)
set(PLATFORM_MACOS OFF)

# Check for R36S cross-compilation
if(DEFINED PLATFORM_R36S OR DEFINED ENV{R36S_BUILD})
    set(PLATFORM_R36S ON)
    message(STATUS "==============================================")
    message(STATUS "Target: R36S Handheld Device")
    message(STATUS "Graphics: OpenGL ES 2.0 via EGL/DRM")
    message(STATUS "==============================================")
    
    add_compile_definitions(PLATFORM_R36S)
    set(OPENGL_VERSION "ES2")
    set(BUILD_TOOLS OFF)  # Force disable tools
    
else()
    # Desktop platforms
    set(PLATFORM_DESKTOP ON)
    set(OPENGL_VERSION "3.2")
    
    if(WIN32)
        set(PLATFORM_WINDOWS ON)
        message(STATUS "Target: Windows Desktop")
        add_compile_definitions(PLATFORM_WINDOWS)
        
    elseif(APPLE)
        set(PLATFORM_MACOS ON)
        message(STATUS "Target: macOS Desktop")
        add_compile_definitions(PLATFORM_MACOS)
        
    elseif(UNIX)
        set(PLATFORM_LINUX ON)
        message(STATUS "Target: Linux Desktop")
        add_compile_definitions(PLATFORM_LINUX)
    endif()
endif()

# Dependencies
message(STATUS "Configuring Dependencies...")

# Engine Source
if(LAMANCHA_BUILD_ENGINE)
    message(STATUS "Configuring Engine Source...")
    if(LAMANCHA_BUILD_TOOLS AND PLATFORM_DESKTOP)
        message(STATUS "Configuring Tools...")
    endif()
    if(LAMANCHA_BUILD_PROJECTS)
        message(STATUS "Configuring Projects...")
    endif()
endif()