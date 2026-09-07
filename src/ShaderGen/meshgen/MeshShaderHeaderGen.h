// MeshShaderHeaderGen.h — GLSL mesh shader 头部生成
//
// 生成 #version、extension、layout、UBO/SSBO 条件包含、
// ColorPalette UBO、gl_InstanceIndex 宏。

#pragma once

#include <hgl/mtl/VertexShaderNodeConfig.h>
#include <hgl/mtl/MaterialStageInterface.h>
#include <hgl/graph/ShaderBufferSources.h>
#include <string>
#include <hgl/mtl/MaterialVertexVaryingConfig.h>

namespace hgl::graph::mtl
{
    inline void EmitMeshShaderVersion(std::string &ms)
    {
        // 460：glslang 仅在 GLSL 4.60 起（或 GL_ARB_shader_draw_parameters）在
        // mesh 阶段符号表声明 gl_DrawID——450 下报 undeclared identifier
        ms += "#version 460\n";
    }

    // RootAddresses push constant block 发射（7 张全局表设备地址）。
    // SSBO 全 BDA 化后的唯一非 descriptor 根入口（无 set 无 binding；CPU 每
    // MaterialBatch push 一次）。字段顺序与 CPU struct RootAddresses 严格一致
    // （ShaderBufferSources.h HGL_ROOT_ADDRESSES_FIELD_LIST 遍历）。
    inline void EmitRootAddressesPushConstant(std::string &ms)
    {
        ms += "layout(push_constant) uniform RootAddresses\n";
        ms += "{\n";
        for (uint32 field_index = 0;
             field_index < kRootAddressesFieldCount;
             ++field_index)
        {
            ms += "    ";
            ms += kRootAddressesFieldGLSLTypes[field_index];
            ms += " ";
            ms += kRootAddressesFieldNames[field_index];
            ms += ";\n";
        }
        ms += "} pc_root;\n";
        ms += "\n";
    }

    inline void EmitMeshShaderExtensions(std::string &ms)
    {
        ms += "#extension GL_EXT_mesh_shader : require\n";
        ms += "#extension GL_EXT_scalar_block_layout : require\n";
        // Arena+BDA：MeshDrawParams 基址字段(uint64_t)与 s1 模块的
        // buffer_reference 声明依赖以下扩展——所有 mesh shader 统一启用
        ms += "#extension GL_EXT_buffer_reference : require\n";
        ms += "#extension GL_ARB_gpu_shader_int64 : require\n";
        ms += "\n";
        // pc_root 紧跟扩展发出（uint64_t 字段依赖 int64 扩展）——行表资源块
        // （MaterialMeshIndexTables，引用 pc_root.addr_l2w_index）由装配器紧随
        // 本 Extension 块之后插入，必须先于它见到 pc_root；HeaderResources 的
        // l2w_ssbo 等 include 同样引用 pc_root。GLSL 无前向引用。
        EmitRootAddressesPushConstant(ms);
    }

    inline void EmitMeshShaderHeaderResources(
        std::string &ms,
        const VertexShaderNodeConfig &node_cfg,
        uint32_t max_invocations,
        uint32_t max_vertices,
        uint32_t max_primitives,
        const bool force_camera_ubo = false)
    {
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

        const bool needs_camera = force_camera_ubo
                               || node_cfg.projection == ProjectionMode::WorldCameraVP
                               || node_cfg.orientation == OrientationMode::CameraFacingFree
                               || node_cfg.orientation == OrientationMode::CameraFacingAxisY;
        // Viewport UBO 无条件 include：它是场景级 UBO（Scene set binding=2），
        // 切换 FBO 必绑、所有材质可用——不再按投影条件裁剪（Line 的 3D 线宽
        // 计算也读 viewport.viewport_resolution，取代 MeshDrawParams.viewport_height）。
        const bool needs_l2w = (node_cfg.orientation == OrientationMode::World ||
                                node_cfg.orientation == OrientationMode::CameraFacingFree ||
                                node_cfg.orientation == OrientationMode::CameraFacingAxisY);

        if (needs_camera)
        {
            ms += "#include \"ubo/camera_info.glsl\"\n";
            ms += "SCENE_CAMERA_UBO;\n";
        }
        ms += "#include \"ubo/viewport_info.glsl\"\n";
        ms += "SCENE_VIEWPORT_UBO;\n";

        if (needs_l2w)
        {
            // l2w_ssbo.glsl：buffer_reference 类型声明 + l2w 垫片宏
            // （地址经 pc_root.addr_l2w 下发——pc_root 已由 EmitRootAddressesPushConstant 先行发射）
            ms += "#include \"common/l2w_ssbo.glsl\"\n";
        }

        ms += "\n";
    }

    // 生成 mesh shader 头部：版本声明、extension、layout、UBO/SSBO 条件包含。
    inline void EmitMeshShaderHeader(
        std::string &ms,
        const VertexShaderNodeConfig &node_cfg,
        uint32_t max_invocations,
        uint32_t max_vertices,
        uint32_t max_primitives,
        const bool force_camera_ubo = false)
    {
        EmitMeshShaderVersion(ms);
        EmitMeshShaderExtensions(ms);
        EmitMeshShaderHeaderResources(
            ms,
            node_cfg,
            max_invocations,
            max_vertices,
            max_primitives,
            force_camera_ubo);
    }

    // MaterialColorPalette UBO（palette 材质）
    inline void EmitColorPaletteUBO(
        std::string &ms,
        const MaterialVertexVaryingConfig &varying_cfg)
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
