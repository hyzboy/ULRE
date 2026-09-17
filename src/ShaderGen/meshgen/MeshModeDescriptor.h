// MeshModeDescriptor.h — Mesh shader 模式正向声明描述符与注册表
//
// 消除 MeshTemplateEmitter 编排器内的特殊分支（如 if (mode == CharQuad)）。
// 每个模式通过描述符正向声明自己的拓扑容量、Defines、UBO 需求集、
// 专属资源、Stage 1 输入、Stage 2 映射、Stage 3 投影以及主循环体发射器。

#pragma once

#include <hgl/mtl/MeshShaderMode.h>
#include <hgl/mtl/VertexShaderNodeConfig.h>
#include <hgl/mtl/VertexNodeConfigResolver.h>
#include <hgl/mtl/MaterialStageInterface.h>
#include <hgl/mtl/MaterialVertexVaryingConfig.h>
#include <hgl/mtl/DescriptorSemantic.h>
#include <hgl/type/OrderedSet.h>
#include <hgl/log/Log.h>
#include <vulkan/vulkan.h>
#include <string>

// 子模块生成器与上下文
#include "MeshShaderHeaderGen.h"
#include "MeshShaderModeVertexPassthrough.h"
#include "MeshShaderModeLineQuad.h"
#include "MeshShaderModeCharQuad.h"

namespace hgl::graph::mtl
{
    struct MeshModeCapacity
    {
        uint32_t max_vertices   = 0;
        uint32_t max_primitives = 0;
    };

    using MeshTopologyResolver = bool (*)(
        uint32_t max_invocations,
        MeshModeCapacity &out_capacity);

    using MeshDefinesEmitter = void (*)(
        std::string &out_glsl,
        const MaterialVertexVaryingConfig &varying_cfg);

    using MeshUboResolver = void (*)(
        const VertexShaderNodeConfig &node_cfg,
        const MaterialVertexVaryingConfig &varying_cfg,
        hgl::OrderedSet<DescriptorSemantic> &out_ubos);

    using MeshResourceEmitter = void (*)(
        std::string &out_glsl,
        const VertexShaderNodeConfig &node_cfg,
        const MaterialVertexVaryingConfig &varying_cfg);

    using MeshStage1Resolver = const char *(*)(
        const VertexShaderNodeConfig &node_cfg,
        VkFormat position_format);

    using MeshStage2MappingResolver = const char *(*)(
        const VertexShaderNodeConfig &node_cfg);

    using MeshStage3ProjectionResolver = const char *(*)(
        const VertexShaderNodeConfig &node_cfg);

    using MeshBodyEmitter = void (*)(
        std::string &out_glsl,
        const MeshShaderModeContext &ctx,
        VkFormat position_format);

    struct MeshModeDescriptor
    {
        MeshShaderMode mode;
        const char *name;
        MeshTopologyResolver resolve_topology;
        MeshDefinesEmitter emit_defines;
        MeshUboResolver resolve_ubos;
        MeshResourceEmitter emit_custom_resources;
        MeshStage1Resolver resolve_stage1_input;
        MeshStage2MappingResolver resolve_stage2_mapping;
        MeshStage3ProjectionResolver resolve_stage3_projection;
        MeshBodyEmitter emit_body;
    };

    // ── Topology Resolvers ──────────────────────────────────────────────
    inline bool ResolveVertexPassthroughTopology(
        uint32_t max_invocations,
        MeshModeCapacity &out_capacity)
    {
        if ((max_invocations % 3u) != 0u)
        {
            GLogError("[ShaderGen] VertexPassthrough 的 max_invocations(%u) 必须是 3 的倍数",
                      max_invocations);
            return false;
        }
        out_capacity.max_vertices   = max_invocations;
        out_capacity.max_primitives = max_invocations / 3u;
        return true;
    }

    inline bool ResolveLineQuadTopology(
        uint32_t max_invocations,
        MeshModeCapacity &out_capacity)
    {
        out_capacity.max_vertices   = max_invocations * 4u;
        out_capacity.max_primitives = max_invocations * 2u;
        return true;
    }

    inline bool ResolveCharQuadTopology(
        uint32_t max_invocations,
        MeshModeCapacity &out_capacity)
    {
        out_capacity.max_vertices   = max_invocations * 4u;
        out_capacity.max_primitives = max_invocations * 2u;
        return true;
    }

    // ── Defines Emitters ────────────────────────────────────────────────
    inline void EmitStandardMeshDefines(
        std::string &out_glsl,
        const MaterialVertexVaryingConfig &varying_cfg)
    {
        EmitGlInstanceIndexMacro(out_glsl);
        if (varying_cfg.use_transform_id_attr)
        {
            out_glsl += "#define HGL_L2W_FROM_VERTEX_ATTR\n";
        }
    }

    // ── UBO Resolvers ───────────────────────────────────────────────────
    inline void ResolveVertexPassthroughUbos(
        const VertexShaderNodeConfig &node_cfg,
        const MaterialVertexVaryingConfig &varying_cfg,
        hgl::OrderedSet<DescriptorSemantic> &out_ubos)
    {
        out_ubos.Add(DescriptorSemantic::ViewportInfo);
        const bool needs_camera = (node_cfg.projection == ProjectionMode::WorldCameraVP)
                               || (node_cfg.orientation == OrientationMode::CameraFacingFree)
                               || (node_cfg.orientation == OrientationMode::CameraFacingAxisY);
        if (needs_camera)
            out_ubos.Add(DescriptorSemantic::CameraInfo);
        if (varying_cfg.emit_vertex_color_from_palette)
            out_ubos.Add(DescriptorSemantic::MaterialColorPalette);
    }

    inline void ResolveLineQuadUbos(
        const VertexShaderNodeConfig &/*node_cfg*/,
        const MaterialVertexVaryingConfig &varying_cfg,
        hgl::OrderedSet<DescriptorSemantic> &out_ubos)
    {
        out_ubos.Add(DescriptorSemantic::ViewportInfo);
        out_ubos.Add(DescriptorSemantic::CameraInfo);
        if (varying_cfg.emit_vertex_color_from_palette)
            out_ubos.Add(DescriptorSemantic::MaterialColorPalette);
    }

    inline void ResolveCharQuadUbos(
        const VertexShaderNodeConfig &/*node_cfg*/,
        const MaterialVertexVaryingConfig &/*varying_cfg*/,
        hgl::OrderedSet<DescriptorSemantic> &out_ubos)
    {
        out_ubos.Add(DescriptorSemantic::ViewportInfo);
    }

    // ── Custom Resource Emitters ────────────────────────────────────────
    inline void EmitCharQuadCustomResources(
        std::string &out_glsl,
        const VertexShaderNodeConfig &/*node_cfg*/,
        const MaterialVertexVaryingConfig &/*varying_cfg*/)
    {
        EmitCharQuadSSBODeclarations(out_glsl);
    }

    // ── Stage 1 Input Resolvers ─────────────────────────────────────────
    inline const char *ResolveStandardStage1Input(
        const VertexShaderNodeConfig &node_cfg,
        VkFormat position_format)
    {
        VertexInputMode effective_input = node_cfg.input;
        if (position_format == VK_FORMAT_R32G32_SFLOAT)
            effective_input = VertexInputMode::Vec2Position;
        else if (position_format == VK_FORMAT_R32G32B32_SFLOAT ||
                 position_format == VK_FORMAT_R32G32B32A32_SFLOAT)
            effective_input = VertexInputMode::Vec3Position;

        if (effective_input == VertexInputMode::Vec2Position)
            return "vertex/s1_position_vec2.glsl";
        return "vertex/s1_position_vec3.glsl";
    }

    // ── Stage 2 Mapping Resolvers (Orthogonal to Stage 3) ───────────────
    inline const char *ResolveStandardStage2Mapping(
        const VertexShaderNodeConfig &node_cfg)
    {
        return VertexNodeConfigResolver::GetMappingModulePath(node_cfg);
    }

    // ── Stage 3 Projection Resolvers (Orthogonal to Stage 2) ────────────
    inline const char *ResolveStandardStage3Projection(
        const VertexShaderNodeConfig &node_cfg)
    {
        return VertexNodeConfigResolver::GetStage3ModulePath(node_cfg);
    }

    // ── Mode Body Emitters ──────────────────────────────────────────────
    inline void EmitVertexPassthroughModeBody(
        std::string &out_glsl,
        const MeshShaderModeContext &ctx,
        VkFormat /*position_format*/)
    {
        EmitVertexPassthroughBody(out_glsl, ctx);
    }

    inline void EmitLineQuadModeBody(
        std::string &out_glsl,
        const MeshShaderModeContext &ctx,
        VkFormat position_format)
    {
        EmitLineQuadBody(out_glsl, ctx, position_format);
    }

    inline void EmitCharQuadModeBody(
        std::string &out_glsl,
        const MeshShaderModeContext &ctx,
        VkFormat /*position_format*/)
    {
        EmitCharQuadBody(out_glsl, ctx);
    }

    // ── Registry Table & Lookup ─────────────────────────────────────────
    inline const MeshModeDescriptor *GetMeshModeDescriptor(MeshShaderMode mode)
    {
        static const MeshModeDescriptor s_descriptors[] = {
            {
                MeshShaderMode::VertexPassthrough,
                "VertexPassthrough",
                ResolveVertexPassthroughTopology,
                EmitStandardMeshDefines,
                ResolveVertexPassthroughUbos,
                nullptr,
                ResolveStandardStage1Input,
                ResolveStandardStage2Mapping,
                ResolveStandardStage3Projection,
                EmitVertexPassthroughModeBody,
            },
            {
                MeshShaderMode::LineQuad,
                "LineQuad",
                ResolveLineQuadTopology,
                EmitStandardMeshDefines,
                ResolveLineQuadUbos,
                nullptr,
                ResolveStandardStage1Input,
                ResolveStandardStage2Mapping,
                ResolveStandardStage3Projection,
                EmitLineQuadModeBody,
            },
            {
                MeshShaderMode::CharQuad,
                "CharQuad",
                ResolveCharQuadTopology,
                nullptr,
                ResolveCharQuadUbos,
                EmitCharQuadCustomResources,
                nullptr,
                nullptr,
                nullptr,
                EmitCharQuadModeBody,
            },
        };

        for (const auto &desc : s_descriptors)
        {
            if (desc.mode == mode)
                return &desc;
        }
        return nullptr;
    }
} // namespace hgl::graph::mtl
