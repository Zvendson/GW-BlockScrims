cmake_minimum_required(VERSION 3.19)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "You are configuring a 64bit build, this is not supported. Run cmake with `-A Win32`")
endif()

project(BlockScrims)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# Outputs dll, exe, and pdb into a /bin/config folder
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/")

add_compile_definitions("NOMINMAX")
add_compile_definitions("WIN32_LEAN_AND_MEAN")
add_compile_definitions(APP_NAME="BlockScrims")
add_compile_definitions(APP_NAMEW=L"BlockScrims")

add_library(BlockScrims SHARED)

file(GLOB SOURCES
    "include/*.h"
    "include/*.embed"
    "src/*.h"
    "src/*.cpp")

target_sources(BlockScrims PUBLIC ${SOURCES})
target_precompile_headers(BlockScrims PRIVATE "src/pch.h")

target_compile_options(BlockScrims PRIVATE /W4 /WX /ZI)

target_include_directories(BlockScrims PUBLIC 
    "include/"
    "src/")

include(directx)
include(imgui)
include(gwca)

target_link_libraries(BlockScrims PRIVATE
    directx
    imgui    
    gwca)
