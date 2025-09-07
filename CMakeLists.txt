cmake_minimum_required(VERSION 3.15)

project(nockerz)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_SUPPRESS_REGENERATION true)

add_subdirectory(noz noz)
    
set(SOURCE_FILES
    src/game.cpp
    src/main.cpp
        src/Archer.cpp
        src/arrow.cpp
        src/player.cpp
        src/world.cpp
    src/game_assets.cpp
)

add_executable(nockerz ${SOURCE_FILES})
set_target_properties(nockerz PROPERTIES WIN32_EXECUTABLE TRUE)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT nockerz)
target_precompile_headers(nockerz PUBLIC src/pch.h)
target_link_libraries(nockerz noz)
target_include_directories(nockerz PRIVATE src)
target_compile_definitions(nockerz PRIVATE GLM_ENABLE_EXPERIMENTAL)

