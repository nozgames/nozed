cmake_minimum_required(VERSION 3.15)

project(nozed)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_subdirectory(noz noz)
target_compile_definitions(noz PUBLIC NOZ_EDITOR)

file(GLOB SRC_FILES
        src/*.cpp
        src/*.h
        src/**/*.cpp
        src/**/*.h
)
set(SOURCE_FILES ${SRC_FILES})

add_executable(nozed ${SOURCE_FILES} res/windows/nozed.rc)

target_compile_definitions(nozed PRIVATE _CRT_SECURE_NO_WARNINGS)
target_link_libraries(nozed PRIVATE
    noz
    debug ${CMAKE_CURRENT_SOURCE_DIR}/external/glslang/glslangd.lib
    debug ${CMAKE_CURRENT_SOURCE_DIR}/external/glslang/SPIRVd.lib
    debug ${CMAKE_CURRENT_SOURCE_DIR}/external/glslang/SPVRemapperd.lib
    debug ${CMAKE_CURRENT_SOURCE_DIR}/external/glslang/SPIRV-Toolsd.lib
    debug ${CMAKE_CURRENT_SOURCE_DIR}/external/glslang/SPIRV-Tools-optd.lib
    optimized ${CMAKE_CURRENT_SOURCE_DIR}/external/glslang/glslang.lib
    optimized ${CMAKE_CURRENT_SOURCE_DIR}/external/glslang/SPIRV.lib
    optimized ${CMAKE_CURRENT_SOURCE_DIR}/external/glslang/SPVRemapper.lib
    optimized ${CMAKE_CURRENT_SOURCE_DIR}/external/glslang/SPIRV-Tools.lib
    optimized ${CMAKE_CURRENT_SOURCE_DIR}/external/glslang/SPIRV-Tools-opt.lib
)

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /WX /std:c++latest")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W4 /WX")
endif()

set_target_properties(nozed PROPERTIES WIN32_EXECUTABLE TRUE)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT nozed)
target_precompile_headers(nozed PUBLIC src/pch.h)
target_include_directories(nozed PRIVATE
    src
    external
    res/windows
    ${CMAKE_CURRENT_SOURCE_DIR}/noz/external
    ${CMAKE_CURRENT_SOURCE_DIR}/external/glslang/include
)
