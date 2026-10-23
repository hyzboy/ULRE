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
        MeshStage1Resolver resolve_stage1_input;
        MeshStage2MappingResolver resolve_stage2_mapping;
        MeshStage3ProjectionResolver resolve_stage3_projection;

        /// 该模式**实际能写出来**的 mesh varying 语义集合。
        ///
        /// 声明侧只声明 kMeshEmittedVaryingSemantics 里的语义，可写性只覆盖
        /// 上面这个子集 —— 二者必须取「声明 ⊆ 可写」。声明了却写不出来的
        /// varying 不会报任何错（GLSL 允许 out 数组元素未写入），FS 侧会静默
        /// 读到未定义值（#9：LineQuad 曾漏写 UV0/WorldPosition/WorldNormal/
        /// StyleID/FragDirection）。因此本表是**硬门**，不是提示：
        /// MeshTemplateEmitter 在发射前校验，不满足即让材质构建失败。
        const InterStageSemantic *writable_semantics;
        int writable_semantic_count;

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

    /// EmitCharQuadSSBODeclarations 声明的 SSBO 一律走 `s1_text_char_quad.glsl`
    /// （模块正文由该模块自己的 `#include` 依赖链覆盖，不再需要 C++ 侧登记）。
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

    // ── Per-Mode Writable Varying Sets ──────────────────────────────────
    // 与 MeshShaderVaryingGen.h 的 EmitVaryingWrites 分支严格对应：
    // 那里少写一个，这里就不能声称可写。

    // PerVertex 分支：DataIndexID / Color / UV0 / Luminance / FragDirection
    // + WorldPosition / WorldNormal（后两者另需 HasWorldSpace 能力门）。
    // StyleID 只有 CharQuad 写（per-char 图元恒值），VertexPassthrough 无。
    inline constexpr InterStageSemantic kWritableVertexPassthroughSemantics[] =
    {
        InterStageSemantic::DataIndexID,
        InterStageSemantic::Color,
        InterStageSemantic::UV0,
        InterStageSemantic::WorldPosition,
        InterStageSemantic::WorldNormal,
        InterStageSemantic::Luminance,
        InterStageSemantic::FragDirection,
    };
    inline constexpr int kCountOfWritableVertexPassthrough =
        static_cast<int>(sizeof(kWritableVertexPassthroughSemantics)
                       / sizeof(kWritableVertexPassthroughSemantics[0]));

    // LineQuad 的模板只读位置/颜色/亮度 SSBO：没有 TexCoord、没有法线、
    // 没有 per-char style。世界位置可由 from_world/to_world 精确给出。
    inline constexpr InterStageSemantic kWritableLineQuadSemantics[] =
    {
        InterStageSemantic::DataIndexID,
        InterStageSemantic::Color,
        InterStageSemantic::WorldPosition,
        InterStageSemantic::Luminance,
    };
    inline constexpr int kCountOfWritableLineQuad =
        static_cast<int>(sizeof(kWritableLineQuadSemantics)
                       / sizeof(kWritableLineQuadSemantics[0]));

    // CharQuad 自带字形 SSBO：UV 来自字形度量、StyleID 来自字形记录。
    // 没有亮度、没有世界空间、没有 frag direction。
    inline constexpr InterStageSemantic kWritableCharQuadSemantics[] =
    {
        InterStageSemantic::DataIndexID,
        InterStageSemantic::Color,
        InterStageSemantic::UV0,
        InterStageSemantic::StyleID,
    };
    inline constexpr int kCountOfWritableCharQuad =
        static_cast<int>(sizeof(kWritableCharQuadSemantics)
                       / sizeof(kWritableCharQuadSemantics[0]));

    /// 找出第一个「会声明成 varying、但该模式写不出来」的语义。
    /// 返回 true 表示契约被破坏，out_semantic 为罪魁（用于报错定位）。
    inline bool FindUnwritableMeshVarying(
        const MeshModeDescriptor &desc,
        const ValueArray<InterStageSemanticContractEntry> &stage_interface,
        InterStageSemantic &out_semantic) noexcept
    {
        for (int i = 0; i < stage_interface.GetCount(); ++i)
        {
            const InterStageSemantic semantic = stage_interface[i].semantic;
            if (!IsMeshEmittedVaryingSemantic(semantic))
                continue;

            bool writable = false;
            for (int w = 0; w < desc.writable_semantic_count; ++w)
            {
                if (desc.writable_semantics[w] == semantic)
                {
                    writable = true;
                    break;
                }
            }

            if (!writable)
            {
                out_semantic = semantic;
                return true;
            }
        }

        return false;
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
                nullptr,
                ResolveStandardStage1Input,
                VertexNodeConfigResolver::GetMappingModulePath,
                VertexNodeConfigResolver::GetStage3ModulePath,
                kWritableVertexPassthroughSemantics,
                kCountOfWritableVertexPassthrough,
                EmitVertexPassthroughBody,
            },
            {
                MeshShaderMode::LineQuad,
                "LineQuad",
                ResolveLineQuadTopology,
                EmitStandardMeshDefines,
                nullptr,
                ResolveStandardStage1Input,
                VertexNodeConfigResolver::GetMappingModulePath,
                VertexNodeConfigResolver::GetStage3ModulePath,
                kWritableLineQuadSemantics,
                kCountOfWritableLineQuad,
                EmitLineQuadBody,
            },
            {
                MeshShaderMode::CharQuad,
                "CharQuad",
                ResolveCharQuadTopology,
                nullptr,
                EmitCharQuadCustomResources,
                nullptr,
                nullptr,
                nullptr,
                kWritableCharQuadSemantics,
                kCountOfWritableCharQuad,
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
