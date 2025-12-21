cmake_minimum_required(VERSION 3.10)
project(lua C)

# Lua core source files
set(LUA_CORE_SRC
    src/lapi.c
    src/lcode.c
    src/lctype.c
    src/ldebug.c
    src/ldo.c
    src/ldump.c
    src/lfunc.c
    src/lgc.c
    src/llex.c
    src/lmem.c
    src/lobject.c
    src/lopcodes.c
    src/lparser.c
    src/lstate.c
    src/lstring.c
    src/ltable.c
    src/ltm.c
    src/lundump.c
    src/lvm.c
    src/lzio.c
)

# Lua library source files
set(LUA_LIB_SRC
    src/lauxlib.c
    src/lbaselib.c
    src/lcorolib.c
    src/ldblib.c
    src/liolib.c
    src/lmathlib.c
    src/loadlib.c
    src/loslib.c
    src/lstrlib.c
    src/ltablib.c
    src/lutf8lib.c
    src/linit.c
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