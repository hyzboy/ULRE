// MeshShaderHeaderGen.h — GLSL mesh shader 头部生成
//
// 生成 #version、extension、layout、UBO/SSBO 条件包含、
// ColorPalette UBO、gl_InstanceIndex 宏。

#pragma once

#include <hgl/mtl/VertexShaderNodeConfig.h>
#include <hgl/mtl/MaterialStageInterface.h>
#include <string>
#include "VertexVaryingConfig.h"

namespace hgl::graph::mtl
{
    // 生成 mesh shader 头部：版本声明、extension、layout、UBO/SSBO 条件包含。
    inline void EmitMeshShaderHeader(
        std::string &ms,
        const VertexShaderNodeConfig &node_cfg,
        uint32_t max_invocations,
        uint32_t max_vertices,
        uint32_t max_primitives)
    {
        // 460：glslang 仅在 GLSL 4.60 起（或 GL_ARB_shader_draw_parameters）在
        // mesh 阶段符号表声明 gl_DrawID——450 下报 undeclared identifier
        ms += "#version 460\n";
        ms += "#extension GL_EXT_mesh_shader : require\n";
        ms += "#extension GL_EXT_scalar_block_layout : require\n";
        ms += "\n";
        ms += "layout(local_size_x = ";
        ms += std::to_string(max_invocations);
        ms += ") in;\n";
        ms += "layout(triangles, max_vertices = ";
        ms += std::to_string(max_vertices);
        ms += ", max_primitives = ";
        ms += std::to_string(max_primitives);
        ms += ") out;\n";
        ms += "\n";

        // ── Descriptor macros ──────────────────────────────────────────────
        ms += "#include \"common/descriptor_macros.glsl\"\n";

        const bool needs_camera = (node_cfg.projection == ProjectionMode::WorldCameraVP ||
                                   node_cfg.orientation == OrientationMode::CameraFacingFree ||
                                   node_cfg.orientation == OrientationMode::CameraFacingAxisY);
        const bool needs_viewport = (node_cfg.projection == ProjectionMode::OrthoViewport ||
                                     node_cfg.projection == ProjectionMode::OrthoThenLocalToWorld ||
                                     node_cfg.scale     == ScaleMode::FixedPixelSize);
        const bool needs_l2w = (node_cfg.orientation == OrientationMode::World ||
                                node_cfg.orientation == OrientationMode::CameraFacingFree ||
                                node_cfg.orientation == OrientationMode::CameraFacingAxisY);

        if (needs_camera || needs_viewport)
        {
            if (needs_camera)
            {
                ms += "#include \"ubo/camera_info.glsl\"\n";
                ms += "SCENE_CAMERA_UBO;\n";
            }
            if (needs_viewport)
            {
                ms += "#include \"ubo/viewport_info.glsl\"\n";
                ms += "SCENE_VIEWPORT_UBO;\n";
            }
        }

        if (needs_l2w)
        {
            ms += "#include \"common/l2w_ssbo.glsl\"\n";
            ms += "L2W_SSBO;\n";
        }

        ms += "\n";
    }

    // MaterialColorPalette UBO（palette 材质）
    inline void EmitColorPaletteUBO(
        std::string &ms,
        const VertexVaryingConfig &varying_cfg)
    {
        if (varying_cfg.emit_vertex_color_from_palette)
        {
            ms += "#include \"ubo/color_palette.glsl\"\n";
            ms += "SCENE_COLOR_PALETTE_UBO;\n";
        }
        ms += "\n";
    }

    // mesh shader 无 gl_InstanceIndex（VS 专属内置）——实例索引 = first_instance + gl_WorkGroupID.y
    inline void EmitGlInstanceIndexMacro(std::string &ms)
    {
        ms += "#define gl_InstanceIndex (pc_vertex_index.first_instance + gl_WorkGroupID.y)\n";
        ms += "\n";
    }
}
