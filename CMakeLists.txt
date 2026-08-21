cmake_minimum_required(VERSION 3.10)
project(TempleRunner CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Raylib paths in MSYS2 UCRT64
set(RAYLIB_INCLUDE_DIR "C:/msys64/ucrt64/include")
set(RAYLIB_LIB_DIR "C:/msys64/ucrt64/lib")

include_directories(${RAYLIB_INCLUDE_DIR})
link_directories(${RAYLIB_LIB_DIR})

add_executable(TempleRunner main.cpp)

target_link_libraries(TempleRunner raylib opengl32 gdi32 winmm)
