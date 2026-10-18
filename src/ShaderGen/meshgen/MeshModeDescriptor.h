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

    using MeshResourceEmitter = void (*)(
        std::string &out_glsl,
        const VertexShaderNodeConfig &node_cfg,
        const MaterialVertexVaryingConfig &varying_cfg);

    using MeshStage1Resolver = const char *(*)(
        const VertexShaderNodeConfig &node_cfg,
        VkFormat position_format);

    using MeshStage2MappingResolver = const char *(*)(
        const VertexShaderNodeConfig &node_cfg) noexcept;

    using MeshStage3ProjectionResolver = const char *(*)(
        const VertexShaderNodeConfig &node_cfg) noexcept;

    /// 模式专属资源里由 C++ 硬编码发射的 `#include` 清单（只给模块路径，
    /// 不带 `#include` 语法）。声明键据此把它们连正文带传递闭包一并折进去。
    ///
    /// 这类 include 不经模块依赖，manifest 覆盖不到，与基座 include 同理必须
    /// 显式登记；否则改那个模块不会让 mesh 声明键失效（命中后校验会兜底，
    /// 但代价是每次都要重新编译 + 一条欠近似告警）。
    /// 与 emit_custom_resources 一一对应：改发射器就必须同步改这里。
    using MeshDeclarationIncludeResolver = const char *const *(*)(int &out_count);

    using MeshBodyEmitter = void (*)(
        std::string &out_glsl,
        const MeshShaderModeContext &ctx);

    struct MeshModeDescriptor
    {
        MeshShaderMode mode;
        const char *name;
        MeshTopologyResolver resolve_topology;
        MeshDefinesEmitter emit_defines;
        MeshResourceEmitter emit_custom_resources;
        MeshDeclarationIncludeResolver resolve_declaration_includes;
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
        out_capacity.max_vertices   = max_invocations * 3u;
        out_capacity.max_primitives = max_invocations;
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

    // ── Custom Resource Emitters ────────────────────────────────────────
    inline void EmitCharQuadCustomResources(
        std::string &out_glsl,
        const VertexShaderNodeConfig &/*node_cfg*/,
        const MaterialVertexVaryingConfig &/*varying_cfg*/)
    {
        EmitCharQuadSSBODeclarations(out_glsl);
    }

    /// EmitCharQuadSSBODeclarations 硬编码发射的模块，逐项与那个函数对齐。
    inline const char *const *ResolveCharQuadDeclarationIncludes(int &out_count)
    {
        static const char *const kCharQuadIncludes[] =
        {
            "vertex/s1_text_char_quad.glsl",
        };

        out_count = static_cast<int>(
            sizeof(kCharQuadIncludes) / sizeof(kCharQuadIncludes[0]));
        return kCharQuadIncludes;
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

    // ── Mode Body Emitters ──────────────────────────────────────────────
    // 模式体发射器统一为 (std::string&, const MeshShaderModeContext&)。
    // 原先此处有 5 个签名适配包装（2 个 1:1 转发到 VertexNodeConfigResolver、
    // 3 个丢弃 position_format）——根因是 MeshBodyEmitter 多带一个 VkFormat。
    // 该值已移入 MeshShaderModeContext.position_format（仅 LineQuad 使用），
    // 于是描述符表可直接引用真实实现，包装层整体消失。

    // ── Registry Table & Lookup ─────────────────────────────────────────
    inline const MeshModeDescriptor *GetMeshModeDescriptor(MeshShaderMode mode)
    {
        static const MeshModeDescriptor s_descriptors[] = {
            {
                MeshShaderMode::VertexPassthrough,
                "VertexPassthrough",
                ResolveVertexPassthroughTopology,
                EmitStandardMeshDefines,
                nullptr,
                nullptr,
                ResolveStandardStage1Input,
                VertexNodeConfigResolver::GetMappingModulePath,
                VertexNodeConfigResolver::GetStage3ModulePath,
                EmitVertexPassthroughBody,
            },
            {
                MeshShaderMode::LineQuad,
                "LineQuad",
                ResolveLineQuadTopology,
                EmitStandardMeshDefines,
                nullptr,
                nullptr,
                ResolveStandardStage1Input,
                VertexNodeConfigResolver::GetMappingModulePath,
                VertexNodeConfigResolver::GetStage3ModulePath,
                EmitLineQuadBody,
            },
            {
                MeshShaderMode::CharQuad,
                "CharQuad",
                ResolveCharQuadTopology,
                nullptr,
                EmitCharQuadCustomResources,
                ResolveCharQuadDeclarationIncludes,
                nullptr,
                nullptr,
                nullptr,
                EmitCharQuadBody,
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
