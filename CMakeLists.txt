cmake_minimum_required(VERSION 3.10)
project(LaManchaEngine)

add_subdirectory(ThirdParty/doctest)
add_subdirectory(ThirdParty/dr_libs)
add_subdirectory(ThirdParty/lua)
add_subdirectory(ThirdParty/lz4)

add_executable(EngineSource/main.cxx)

target_link_libraries(LaManchaEngine PRIVATE lua)
add_subdirectory(LaManchaEngine PRIVATE dr_libs)