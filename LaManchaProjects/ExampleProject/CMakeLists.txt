cmake_minimum_required(VERSION 3.15)

# > Read Build Configuration INI <
# ================================

include(${CMAKE_CURRENT_SOURCE_DIR}/CMake/ReadINI.cmake)

message(STATUS "> Reading config/build.ini ...")

read_ini_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config/build.ini" 
    "BUILD"
)

project(PROJECT_NAME VERSION PROJECT_NAME)

# Extract configuration
set(TARGET_PLATFORM ${BUILD_TARGET_PLATFORM})
set(GRAPHICS_BACKEND ${BUILD_TARGET_GRAPHICS_BACKEND})

message(STATUS "========================================")
message(STATUS "Project: ${PROJECT_NAME}")
message(STATUS "Target: ${TARGET_PLATFORM}")
message(STATUS "Graphics: ${GRAPHICS_BACKEND}")
message(STATUS "========================================")

# > Parse Source Directories from INI <
# =====================================

message(STATUS "> Parsing source directories...")

# Directories may include multiple sources ("src, src/game, src/entities") so they will be parsed everytime
string(REPLACE "," ";" INCLUDE_DIR_LIST "${BUILD_DIRECTORIES_INCLUDE_DIRS}")
string(REPLACE " " "" INCLUDE_DIR_LIST "${INCLUDE_DIR_LIST}")  # Remove spaces

set(PROJECT_SOURCES "")

foreach(DIR ${INCLUDE_DIR_LIST})
    message(STATUS "========================================")
    message(STATUS "From ${DIR} including the following files:")

    file(GLOB_RECURSE DIR_SOURCES 
        "${CMAKE_CURRENT_SOURCE_DIR}/${DIR}/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/${DIR}/*.c"
    )
    list(APPEND PROJECT_SOURCES ${DIR_SOURCES})
    message(STATUS "${DIR_SOURCES}")

    message(STATUS "========================================")
endforeach()

# Exclude directories
if(DEFINED BUILD_DIRECTORIES_EXCLUDE_DIRS)
    string(REPLACE "," ";" EXCLUDE_DIR_LIST "${BUILD_DIRECTORIES_EXCLUDE_DIRS}")
    
    message(STATUS "========================================")
    message(STATUS "Excluded directories:")

    foreach(EXCLUDE_DIR ${EXCLUDE_DIR_LIST})
        string(STRIP "${EXCLUDE_DIR}" EXCLUDE_DIR)
        
        message(STATUS "${EXCLUDE_DIR}")

        list(FILTER PROJECT_SOURCES EXCLUDE REGEX 
            "${CMAKE_CURRENT_SOURCE_DIR}/${EXCLUDE_DIR}/.*")
    endforeach()

    message(STATUS "========================================")
endif()

message(STATUS "Source files found: ${PROJECT_SOURCES}")

# > Find Pre-Built Engine Library <
# =================================

# Point to appropriate engine build
set(ENGINE_BUILD_DIR "${ENGINE_DIR}/bin-${TARGET_PLATFORM}")

if(NOT EXISTS "${ENGINE_BUILD_DIR}")
    message(FATAL_ERROR 
        "Engine not built for ${TARGET_PLATFORM}!\n"
        "Expected: ${ENGINE_BUILD_DIR}\n"
        "Run: cd ../../ && mkdir build-${TARGET_PLATFORM} && "
        "cd build-${TARGET_PLATFORM} && cmake .. && make")
endif()

# Add engine library
message(STATUS "> Exporting Engine...")
add_library(LaManchaEngine STATIC IMPORTED)
set_target_properties(LaManchaEngine PROPERTIES
    IMPORTED_LOCATION "${ENGINE_BUILD_DIR}/bin/LaManchaEngine.a"
    INTERFACE_INCLUDE_DIRECTORIES "${ENGINE_DIR}/EngineSource"
)

# > Platform-Specific Dependencies <
# ==================================

if(TARGET_PLATFORM STREQUAL "r36s")
    message(STATUS "> Setting up R36S-specific dependencies...")
    # R36S needs system graphics libraries
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(EGL REQUIRED egl)
    pkg_check_modules(DRM REQUIRED libdrm)
    pkg_check_modules(GBM REQUIRED gbm)
    
    set(PLATFORM_LIBS
        ${EGL_LIBRARIES}
        ${DRM_LIBRARIES}
        ${GBM_LIBRARIES}
    )
    
    # Tell engine to use EGL backend
    add_compile_definitions(USE_EGL_BACKEND)
    
elseif(TARGET_PLATFORM STREQUAL "windows")
    message(STATUS "> Setting up Windows Desktop-specific dependencies...")
    add_compile_definitions(USE_GLFW_BACKEND)
    set(PLATFORM_LIBS)  # GLFW already linked in engine
    
elseif(TARGET_PLATFORM STREQUAL "linux")
    message(STATUS "> Setting up Linux Desktop-specific dependencies...")
    add_compile_definitions(USE_GLFW_BACKEND)
    set(PLATFORM_LIBS pthread dl m)
endif()

# > Create Game Executable <
# ==========================

message(STATUS "> Generating executable...")
add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE
    LaManchaEngine
    ${PLATFORM_LIBS}
)

# > Copy Runtime Configuration Files <
# ====================================

# Copy engine.ini and input.ini to build directory
# These are read by the engine/game at runtime
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/config"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/config"
    COMMENT "> Copying runtime configuration files..."
)

# > Asset Packaging (Based on build.ini) <
# ========================================

message(STATUS "> Parsing asset directories...")
string(REPLACE "," ";" ASSET_DIRS "${BUILD_DIRECTORIES_INCLUDE_ASSETS}")
if(BUILD_PACKAGING_CREATE_PAK)
    # Parse asset directories
    
    set(ASSET_INPUT_ARGS "")

    message(STATUS "========================================")
    foreach(ASSET_DIR ${ASSET_DIRS})
        message(STATUS "Included directory: ${ASSET_DIR}")

        string(STRIP "${ASSET_DIR}" ASSET_DIR)
        list(APPEND ASSET_INPUT_ARGS "--input" "${CMAKE_CURRENT_SOURCE_DIR}/${ASSET_DIR}")
        message(STATUS "${ASSET_DIR}")
    endforeach()
    message(STATUS "========================================")
    message(STATUS "Included the following assets: ${ASSET_INPUT_ARGS}")
    message(STATUS "========================================")
    
    # Determine pak builder location
    set(PAK_BUILDER "${CMAKE_CURRENT_SOURCE_DIR}/../../build-native/bin/LaManchaPakBuilder")
    if(WIN32)
        set(PAK_BUILDER "${PAK_BUILDER}.exe")
    endif()
    
    # Build pak file
    message(STATUS "> Creating PAK file...")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${PAK_BUILDER}
            ${ASSET_INPUT_ARGS}
            --output "$<TARGET_FILE_DIR:${PROJECT_NAME}>/${BUILD_PACKAGING_PAK_FILENAME}"
            $<$<BOOL:${BUILD_PACKAGING_COMPRESS_ASSETS}>:--compress>
            --format "${BUILD_PACKAGING_COMPRESSION_FORMAT}"
        COMMENT "> Packaging game assets into ${BUILD_PACKAGING_PAK_FILENAME}..."
    )
else()
    # Just copy assets as-is    
    message(STATUS "> Copying assets...")
    foreach(ASSET_DIR ${ASSET_DIRS})
        string(STRIP "${ASSET_DIR}" ASSET_DIR)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/${ASSET_DIR}"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/${ASSET_DIR}"
            COMMENT "> Copying ${ASSET_DIR}..."
        )
    endforeach()
endif()

message(STATUS "Project configured successfully")