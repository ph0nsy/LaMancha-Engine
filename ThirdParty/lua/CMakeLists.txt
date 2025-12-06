cmake_minimum_required(VERSION 3.10)
project(lua C)

# Lua core source files
set(LUA_CORE_SRC
    lapi.c
    lcode.c
    lctype.c
    ldebug.c
    ldo.c
    ldump.c
    lfunc.c
    lgc.c
    llex.c
    lmem.c
    lobject.c
    lopcodes.c
    lparser.c
    lstate.c
    lstring.c
    ltable.c
    ltm.c
    lundump.c
    lvm.c
    lzio.c
)

# Lua library source files
set(LUA_LIB_SRC
    lauxlib.c
    lbaselib.c
    lcorolib.c
    ldblib.c
    liolib.c
    lmathlib.c
    loadlib.c
    loslib.c
    lstrlib.c
    ltablib.c
    lutf8lib.c
    linit.c
)

# Create static library
add_library(lua STATIC ${LUA_CORE_SRC} ${LUA_LIB_SRC})

# Set C standard
set_property(TARGET lua PROPERTY C_STANDARD 99)

# Platform-specific settings
if(UNIX)
    target_compile_definitions(lua PUBLIC LUA_USE_POSIX)
    target_link_libraries(lua PUBLIC m dl)
endif()

if(UNIX AND NOT APPLE)
    target_compile_definitions(lua PUBLIC LUA_USE_LINUX)
endif()

if(WIN32)
    target_compile_definitions(lua PUBLIC LUA_USE_WINDOWS)
endif()

# Include directory
target_include_directories(lua PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})