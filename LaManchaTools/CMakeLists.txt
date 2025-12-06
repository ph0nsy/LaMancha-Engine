message(STATUS "Configuring LaMancha Tools...")

# Tools require desktop environment
if(NOT WIN32 AND NOT APPLE AND NOT UNIX)
    message(FATAL_ERROR "Tools can only be built on desktop platforms")
endif()

# > Find Engine Library <
# =======================

if(DEFINED LaManchaEngine_DIR) # Engine should be pre-built
    message(STATUS "Using engine from: ${LaManchaEngine_DIR}")
    
    # Add engine as imported target
    add_library(LaManchaEngine STATIC IMPORTED)
    
    if(WIN32)
        set_target_properties(LaManchaEngine PROPERTIES
            IMPORTED_LOCATION "${LaManchaEngine_DIR}/lib/Release/LaManchaEngine.lib"
            INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/EngineSource"
        )
    else()
        set_target_properties(LaManchaEngine PROPERTIES
            IMPORTED_LOCATION "${LaManchaEngine_DIR}/lib/LaManchaEngine.a"
            INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/EngineSource"
        )
    endif()
else()
    message(FATAL_ERROR "LaManchaEngine_DIR not set. Build engine first.")
endif()

# > Common Tool Dependencies <
# ============================

# Ensure ImGui and tinyfiledialogs are available
if(NOT TARGET imgui)
    message(FATAL_ERROR "ImGui not found - required for tools")
endif()

if(NOT TARGET tinyfiledialogs)
    message(FATAL_ERROR "tinyfiledialogs not found - required for tools")
endif()

# [Tool] > Unified LaMancha Tools GUI <
# =====================================

set(TOOLS_SOURCES
    # Main application
    main.cpp
    
    # Project Creator module
    src/projectCreator.h
    src/projectCreator.cpp
    
    # Config Editor module
    src/config_editor.h
    src/config_editor.cpp
    
    # Pak Builder module
    src/pakBuilderGUI.h
    src/pakBuilderGUI.cpp
    
    # Build Manager module
    src/buildManager.h
    src/buildManager.cpp
)

add_executable(LaManchaTools ${TOOLS_SOURCES})

target_link_libraries(LaManchaTools PRIVATE
    LaManchaEngine
    imgui
    miniz
    tinyfiledialogs
    glfw
    glad
)

target_include_directories(LaManchaTools PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/ConfigEditor/src
    ${CMAKE_SOURCE_DIR}/EngineSource
)

# Copy config templates
add_custom_command(TARGET LaManchaTools POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/ConfigEditor/config"
        "$<TARGET_FILE_DIR:LaManchaTools>/config"
    COMMENT "Copying tool configuration files..."
)

# > Set Output Directories <
# ==========================

set_target_properties(LaManchaTools PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
)

message(STATUS "Tools configured successfully")