cmake_minimum_required(VERSION 3.15)

project(nozed)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_subdirectory(noz noz)
    
set(SOURCE_FILES
    src/asset_editor/asset_editor.cpp
    src/asset_editor/grid.cpp
    src/asset_editor/gltf_load.cpp
    src/asset_editor/gltf_save.cpp
    src/asset_editor/game_assets.cpp
    src/asset_editor/editable_asset.cpp
    src/asset_editor/editable_mesh.cpp
    src/asset_editor/undo.cpp
    src/asset_manifest.cpp
    src/editor_server.cpp
    src/editor.cpp
    src/file_watcher.cpp
    src/gltf.cpp
    src/importer.cpp
    src/importers/mesh_importer.cpp
    src/importers/font_importer.cpp
    src/importers/shader_importer.cpp
    src/importers/animation_importer.cpp
    src/importers/skeleton_importer.cpp
    src/importers/style_sheet_importer.cpp
    src/importers/vfx_importer.cpp
    src/importers/texture_importer.cpp
    src/importers/sound_importer.cpp
    src/msdf/Contour.cpp
    src/msdf/Edge.cpp
    src/msdf/Shape.cpp
    src/msdf/Math.cpp
    src/msdf/msdf.cpp
    src/msdf/SignedDistance.cpp
    src/rect_packer.cpp
    src/props.cpp
    src/tokenizer.cpp
    src/ttf/TrueTypeFont.cpp
    src/ttf/TrueTypeFontReader.cpp
    src/tui/screen.cpp
    src/tui/text_input.cpp
    src/tui/tstring.cpp
    src/tui/windows_terminal.cpp
    src/views/log_view.cpp
    src/file_helpers.cpp
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
