cmake_minimum_required(VERSION 3.15)

project(meshz)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_subdirectory(noz noz)
    
set(SOURCE_FILES
    src/main.cpp
    src/view.cpp
    src/game_assets.cpp
    src/editable_mesh.cpp
)

add_executable(meshz ${SOURCE_FILES})

set_target_properties(meshz PROPERTIES WIN32_EXECUTABLE TRUE)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT meshz)
target_precompile_headers(meshz PUBLIC src/pch.h)
target_link_libraries(meshz noz)
target_include_directories(meshz PRIVATE src)
