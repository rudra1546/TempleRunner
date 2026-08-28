cmake_minimum_required(VERSION 3.10)
project(TempleRunner CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(EMSCRIPTEN)
    set(PLATFORM "Web" CACHE STRING "Raylib platform target" FORCE)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
    set(CMAKE_C_FLAGS_RELEASE "-Os" CACHE STRING "" FORCE)
    set(CMAKE_CXX_FLAGS_RELEASE "-Os" CACHE STRING "" FORCE)
    
    add_subdirectory(third_party/raylib)

    add_executable(TempleRunner main.cpp)
    target_link_libraries(TempleRunner raylib)
    
    set_target_properties(TempleRunner PROPERTIES
        OUTPUT_NAME "index"
        SUFFIX ".html"
    )
    
    target_link_options(TempleRunner PRIVATE
        "-Os"
        "-sUSE_GLFW=3"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sTOTAL_MEMORY=67108864"
        "--preload-file" "${CMAKE_CURRENT_SOURCE_DIR}/assets@assets"
        "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']"
    )
else()
    # Native Windows Desktop build
    set(RAYLIB_INCLUDE_DIR "C:/msys64/ucrt64/include")
    set(RAYLIB_LIB_DIR "C:/msys64/ucrt64/lib")

    include_directories(${RAYLIB_INCLUDE_DIR})
    link_directories(${RAYLIB_LIB_DIR})

    add_executable(TempleRunner main.cpp)
    target_link_libraries(TempleRunner raylib opengl32 gdi32 winmm)
endif()
