cmake_minimum_required(VERSION 3.15)

project(nozed)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_subdirectory(noz noz)
    
set(SOURCE_FILES
    src/asset_editor/asset_editor.cpp
    src/asset_editor/mesh_editor.cpp
    src/asset_editor/skeleton_editor.cpp
    src/asset_editor/grid.cpp
    src/asset_editor/notifications.cpp

    src/asset/editor_asset.cpp
    src/asset/editor_skeleton.cpp
    src/asset/editor_mesh.cpp
    src/asset/editor_vfx.cpp

    src/utils/file_watcher.cpp
    src/utils/rect_packer.cpp
    src/utils/props.cpp
    src/utils/tokenizer.cpp
    src/utils/file_helpers.cpp
    src/utils/draw_utils.cpp

    src/import/importer.cpp
    src/import/mesh_importer.cpp
    src/import/font_importer.cpp
    src/import/shader_importer.cpp
    src/import/animation_importer.cpp
    src/import/skeleton_importer.cpp
    src/import/style_sheet_importer.cpp
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
    src/editor_assets.cpp
    src/editor_server.cpp
    src/editor.cpp
    src/commands.cpp
)


add_executable(nozed ${SOURCE_FILES})

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


#set_target_properties(nozed PROPERTIES WIN32_EXECUTABLE TRUE)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT nozed)
target_precompile_headers(nozed PUBLIC src/pch.h)
target_include_directories(nozed PRIVATE
    src
    external
    ${CMAKE_CURRENT_SOURCE_DIR}/noz/external
    ${CMAKE_CURRENT_SOURCE_DIR}/external/glslang/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/enet/include
)
