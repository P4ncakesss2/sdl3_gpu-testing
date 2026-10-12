cmake_minimum_required(VERSION 3.24)
project(SDL3GPUTriangle LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SDL_SHARED OFF CACHE BOOL "Build SDL as a shared library" FORCE)
set(SDL_STATIC ON CACHE BOOL "Build SDL as a static library" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "Build SDL test library" FORCE)
set(SDL_TESTS OFF CACHE BOOL "Build SDL tests" FORCE)
# Keep the game-facing SDL subsystems, but skip features this project does not use.
set(SDL_RENDER ON CACHE BOOL "Build SDL's 2D renderer subsystem" FORCE)
set(SDL_CAMERA OFF CACHE BOOL "Build SDL camera support" FORCE)
set(SDL_SENSOR OFF CACHE BOOL "Build SDL sensor support" FORCE)
set(SDL_TRAY OFF CACHE BOOL "Build SDL system tray support" FORCE)
set(SDL_NOTIFICATION OFF CACHE BOOL "Build SDL notification support" FORCE)
set(SDL_GPU_OPENXR OFF CACHE BOOL "Build SDL_GPU OpenXR support" FORCE)
add_subdirectory(external/SDL EXCLUDE_FROM_ALL)

add_library(cglm INTERFACE)
target_include_directories(cglm SYSTEM INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/external/cglm/include"
)

set(SDLSHADERCROSS_VENDORED ON CACHE BOOL "Build shadercross dependencies from its submodules" FORCE)
set(SDLSHADERCROSS_DXC ON CACHE BOOL "Enable HLSL compilation via DXC" FORCE)
set(SDLSHADERCROSS_STATIC ON CACHE BOOL "Build shadercross as a static library" FORCE)
set(SDLSHADERCROSS_SHARED OFF CACHE BOOL "Build shadercross as a shared library" FORCE)
set(SDLSHADERCROSS_SPIRVCROSS_SHARED ON CACHE BOOL "Build SPIRV-Cross as a shared dependency" FORCE)
set(SDLSHADERCROSS_CLI ON CACHE BOOL "Build shadercross command-line utility" FORCE)
set(SDLSHADERCROSS_INSTALL OFF CACHE BOOL "Install shadercross" FORCE)
set(SDLSHADERCROSS_TESTS OFF CACHE BOOL "Build shadercross tests" FORCE)
add_subdirectory(external/SDL_shadercross EXCLUDE_FROM_ALL)

set(IMGUI_STATIC yes CACHE STRING "Build cimgui as a static library" FORCE)
set(CIMGUI_TEST no CACHE STRING "Disable cimgui's standalone tests" FORCE)
add_subdirectory(external/cimgui EXCLUDE_FROM_ALL)

file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)
add_executable(sdl3_gpu_triangle ${PROJECT_SOURCES})
target_sources(sdl3_gpu_triangle PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/external/cimgui/imgui/backends/imgui_impl_sdl3.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/cimgui/imgui/backends/imgui_impl_sdlgpu3.cpp"
)
target_include_directories(sdl3_gpu_triangle PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(sdl3_gpu_triangle PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/external/stb")
target_include_directories(sdl3_gpu_triangle PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/external/cimgui/imgui/backends")
target_compile_definitions(sdl3_gpu_triangle PRIVATE CIMGUI_DEFINE_ENUMS_AND_STRUCTS=1)
target_link_libraries(sdl3_gpu_triangle PRIVATE
    SDL3::SDL3-static
    cimgui
    cglm
)

file(GLOB SHADER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.vert.hlsl"
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag.hlsl"
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.comp.hlsl"
)
if(NOT SHADER_SOURCES)
    message(FATAL_ERROR "No .vert.hlsl, .frag.hlsl, or .comp.hlsl files found in shaders/")
endif()

set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY "${SHADER_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/assets")
add_custom_target(copy_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "${CMAKE_CURRENT_BINARY_DIR}/assets"
    COMMENT "Copying assets"
    VERBATIM
)
add_dependencies(sdl3_gpu_triangle copy_assets)
set(SHADER_OUTPUTS)
foreach(SHADER_SOURCE IN LISTS SHADER_SOURCES)
    get_filename_component(SHADER_FILENAME "${SHADER_SOURCE}" NAME)
    string(REGEX REPLACE "\\.hlsl$" "" SHADER_STEM "${SHADER_FILENAME}")
    if(SHADER_FILENAME MATCHES "\\.vert\\.hlsl$")
        set(SHADER_STAGE vertex)
    elseif(SHADER_FILENAME MATCHES "\\.comp\\.hlsl$")
        set(SHADER_STAGE compute)
    else()
        set(SHADER_STAGE fragment)
    endif()

    foreach(SHADER_FORMAT IN ITEMS SPIRV DXIL MSL)
        string(TOLOWER "${SHADER_FORMAT}" SHADER_FORMAT_LOWER)
        set(SHADER_OUTPUT "${SHADER_OUTPUT_DIR}/${SHADER_STEM}.${SHADER_FORMAT_LOWER}")
        add_custom_command(
            OUTPUT "${SHADER_OUTPUT}"
            COMMAND $<TARGET_FILE:shadercross>
                    "${SHADER_SOURCE}"
                    --dest "${SHADER_FORMAT}"
                    --stage "${SHADER_STAGE}"
                    --entrypoint main
                    --output "${SHADER_OUTPUT}"
            DEPENDS shadercross "${SHADER_SOURCE}"
            VERBATIM
        )
        list(APPEND SHADER_OUTPUTS "${SHADER_OUTPUT}")
    endforeach()
endforeach()
add_custom_target(compile_shaders DEPENDS ${SHADER_OUTPUTS})
add_dependencies(sdl3_gpu_triangle compile_shaders)
