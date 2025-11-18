cmake_minimum_required(VERSION 3.15)

project(nozed)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_subdirectory(noz noz)
    
set(SOURCE_FILES

    src/grid.cpp
    src/build.cpp
    src/view.cpp
    src/notifications.cpp

    src/asset/animation_editor.cpp
    src/asset/animation_data.cpp
    src/asset/asset_data.cpp
    src/asset/mesh_data.cpp
    src/asset/mesh_editor.cpp
    src/asset/texture_data.cpp
    src/asset/texture_editor.cpp
    src/asset/skeleton_data.cpp
    src/asset/skeleton_editor.cpp
    src/asset/shader_data.cpp
    src/asset/sound_data.cpp
    src/asset/font_data.cpp
    src/asset/vfx_data.cpp

    src/utils/file_watcher.cpp
    src/utils/rect_packer.cpp
    src/utils/props.cpp
    src/utils/tokenizer.cpp
    src/utils/file_helpers.cpp
    src/utils/draw_utils.cpp
    src/utils/ui_utils.cpp

    src/tools/box_select.cpp
    src/tools/move.cpp
    src/tools/scale.cpp
    src/tools/rotate.cpp
    src/tools/select.cpp
    src/tools/weight.cpp

    src/import/importer.cpp
    src/import/mesh_importer.cpp
    src/import/font_importer.cpp
    src/import/shader_importer.cpp
    src/import/animation_importer.cpp
    src/import/skeleton_importer.cpp
    src/import/vfx_importer.cpp
    src/import/texture_importer.cpp
    src/import/sound_importer.cpp
    src/import/asset_manifest.cpp

    src/msdf/Contour.cpp
    src/msdf/Edge.cpp
    src/msdf/Shape.cpp
    src/msdf/Math.cpp
    src/msdf/msdf.cpp
    src/msdf/SignedDistance.cpp

    src/ttf/TrueTypeFont.cpp
    src/ttf/TrueTypeFontReader.cpp

    src/shortcut.cpp
    src/undo.cpp
    src/nozed_assets.cpp
    src/editor_server.cpp
    src/editor.cpp
    src/command.cpp
)


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
    ${CMAKE_CURRENT_SOURCE_DIR}/noz/external/enet/enet.lib
)

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /WX")
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
    ${CMAKE_CURRENT_SOURCE_DIR}/external/enet/include
)
