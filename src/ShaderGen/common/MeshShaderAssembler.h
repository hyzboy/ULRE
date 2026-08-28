// MeshShaderAssembler.h — 通用 mesh shader 生成器（调度入口）
//
// 生成 GLSL 骨架，顶点输出走 mesh 图元：
//   - 通用模式（默认）：每线程 1 顶点，直通到 gl_MeshVerticesEXT（模拟 VS 行为）
//   - Line quad 模式：每线程 1 线段，展开成 quad（2 三角形）
//
// 复用现有 s1_* 模块（LoadVertexData 读 SSBO）——通过宏把 gl_VertexIndex
// 替换为 gl_LocalInvocationIndex（mesh shader 无 gl_VertexIndex）。
//
// 输出契约：varying/descriptor/Stage1/2/3 结构，
// 由 CompileCompositorMaterial 注入 binding_preamble + 索引表声明。

#pragma once

#include <hgl/mtl/MeshShaderMode.h>
#include <hgl/mtl/VertexShaderNodeConfig.h>
#include <hgl/mtl/VertexNodeConfigResolver.h>
#include <hgl/mtl/MaterialStageInterface.h>
#include <vulkan/vulkan.h>
#include <string>
#include "VertexVaryingConfig.h"   // mtl::VertexVaryingConfig

// 子模块
#include "MeshShaderHeaderGen.h"
#include "MeshShaderVertexAdapter.h"
#include "MeshShaderVaryingGen.h"
#include "MeshShaderModeVertexPassthrough.h"
#include "MeshShaderModeLineQuad.h"
#include "MeshShaderModeCharQuad.h"

namespace hgl::graph::mtl
{
    // MeshShaderAssembler — 生成 mesh stage GLSL
    //
    // 参数说明：
    //   max_invocations — threadgroup 大小（受设备 maxMeshWorkGroupSizeX 限制）
    //
    // 通用模式输出拓扑：triangle list（每 3 连续顶点 1 三角形）
    // Line 模式输出拓扑：triangle list（每线段 4 顶点 2 三角形）
    // MeshShaderMode 枚举定义见 inc/hgl/mtl/MeshShaderMode.h（MaterialDefinition 共享）

    inline std::string GenerateMeshShader(
        const VertexShaderNodeConfig &node_cfg,
        const VertexVaryingConfig    &varying_cfg,
        VkFormat                      position_format,
        const std::string            &shader_lib_path,
        const MeshShaderMode          mode = MeshShaderMode::VertexPassthrough,
        const uint32_t                max_invocations = 64,
        const std::string            &resolved_input_glsl = {},
        const std::string            &provider_glsl = {},
        const ValueArray<InterStageSemanticContractEntry>
            *resolved_stage_interface = nullptr,
        const PrimitiveType           primitive_type = PrimitiveType::Triangles)
    {
        std::string ms;
        ms.reserve(3072);

        // ── 拓扑/容量 ──────────────────────────────────────────────────────
        uint32_t verts_per_inv = 1;
        uint32_t prims_per_inv = 0;

        switch (mode)
        {
        case MeshShaderMode::VertexPassthrough:
            verts_per_inv = 1;
            prims_per_inv = 0;   // 三角形由 3 线程组填索引
            break;
        case MeshShaderMode::LineQuad:
            verts_per_inv = 4;
            prims_per_inv = 2;
            break;
        case MeshShaderMode::CharQuad:
            verts_per_inv = 6;    // 6 vertices per character (2 triangles)
            prims_per_inv = 2;    // 2 triangles per character
            break;
        }

        const uint32_t max_vertices    = max_invocations * verts_per_inv;
        const uint32_t max_primitives  = max_invocations * ((prims_per_inv > 0) ? prims_per_inv : 1);

        // ── Header ─────────────────────────────────────────────────────────
        EmitMeshShaderHeader(ms, node_cfg, max_invocations, max_vertices, max_primitives);

        // ── Stage 1: 顶点输入（SSBO）───────────────────────────────────────
        if (mode != MeshShaderMode::CharQuad)
        {
            // 顶点适配层（MeshVertexIndex + VertexIndex SSBO + MeshDrawParams SSBO）
            EmitVertexAdapter(ms);

            // 顶点输入模块（resolved_input_glsl 或默认 s1_position_*）
            if (!resolved_input_glsl.empty())
            {
                ms += resolved_input_glsl;
                if (resolved_input_glsl.back() != '\n')
                    ms += "\n";
            }
            else
            {
                // 非索引直通：Position 从 SSBO 读（s1_position_* 模块）
                // Vec2Position → s1_position_vec2；Vec3Position → s1_position_vec3
                VertexInputMode effective_input = node_cfg.input;
                if (position_format == VK_FORMAT_R32G32_SFLOAT)
                    effective_input = VertexInputMode::Vec2Position;
                else if (position_format == VK_FORMAT_R32G32B32_SFLOAT ||
                         position_format == VK_FORMAT_R32G32B32A32_SFLOAT)
                    effective_input = VertexInputMode::Vec3Position;

                switch (effective_input)
                {
                case VertexInputMode::Vec2Position:
                    ms += "#include \"vertex/s1_position_vec2.glsl\"\n";
                    break;
                case VertexInputMode::Vec3Position:
                default:
                    ms += "#include \"vertex/s1_position_vec3.glsl\"\n";
                    break;
                }
            }

            // 额外顶点属性（color/UV 等 provider 模块）
            if (!provider_glsl.empty())
            {
                ms += provider_glsl;
                if (provider_glsl.back() != '\n') ms += "\n";
            }

            // MaterialColorPalette UBO（palette 材质）
            EmitColorPaletteUBO(ms, varying_cfg);

            // gl_InstanceIndex 宏
            EmitGlInstanceIndexMacro(ms);

            // ── Stage 2: 位置映射 ─────────────────────────────────────────
            // CharQuad 在 main() 中直接用 viewport.ortho_matrix 做坐标变换，不需要 Stage2/3 模块
            const char *stage2_module = VertexNodeConfigResolver::GetMappingModulePath(node_cfg);
            if (!stage2_module)
                return {};   // 未映射的 position_mapping = 映射缺失，硬失败（编译失败）而非静默错渲
            ms += "#include \"" + std::string(stage2_module) + "\"\n";

            ms += "\n";

            // ── Stage 3: 变换策略 ──────────────────────────────────────────
            if (varying_cfg.use_transform_id_attr)
                ms += "#define HGL_L2W_FROM_VERTEX_ATTR\n";

            ms += "#include \"" + std::string(VertexNodeConfigResolver::GetStage3ModulePath(node_cfg)) + "\"\n";
            ms += "\n";
        }
        else
        {
            // CharQuad：仅需顶点适配层
            EmitVertexAdapter(ms);
        }

        // ── Varying 输出（per-vertex 数组，mesh shader 要求）──────────────
        ValueArray<InterStageSemanticContractEntry> adapted_stage_interface;
        MaterialStageInterfaceDiagnostic stage_interface_diagnostic{};
        if (!resolved_stage_interface)
        {
            MaterialVertexVaryingConfig material_varying{};
            material_varying.emit_data_index_id   = varying_cfg.emit_data_index_id;
            material_varying.emit_vertex_color    = varying_cfg.emit_vertex_color;
            material_varying.emit_uv0             = varying_cfg.emit_uv0;
            material_varying.emit_world_pos       = varying_cfg.emit_world_pos;
            material_varying.emit_world_normal    = varying_cfg.emit_world_normal;
            material_varying.emit_luminance       = varying_cfg.emit_luminance;
            material_varying.emit_frag_direction  = varying_cfg.emit_frag_direction;
            material_varying.emit_vertex_color_from_palette =
                varying_cfg.emit_vertex_color_from_palette;
            material_varying.emit_style_id      = varying_cfg.emit_style_id;
            if (!BuildMaterialStageInterface(material_varying,
                                             adapted_stage_interface,
                                             stage_interface_diagnostic))
                return {};
            resolved_stage_interface = &adapted_stage_interface;
        }

        // varying 声明为数组（按顶点索引）
        EmitVaryingDeclarations(ms, *resolved_stage_interface, max_vertices);

        // CharQuad SSBO 声明必须在全局作用域（void main 之前）
        if (mode == MeshShaderMode::CharQuad)
            EmitCharQuadSSBODeclarations(ms);

        ms += "\nvoid main()\n{\n";

        // per-draw 参数行加载（两模式统一）：间接合批经 gl_DrawID 定位本命令行，
        // 直接绘制 gl_DrawID=0 → row 0（CPU 侧保证 row 0 = 本 draw 参数）
        ms += "    pc_vertex_index = sbo_draw_params.rows[gl_DrawID];\n";
        ms += "\n";

        // ── 每线程处理 ─────────────────────────────────────────────────────
        // 构建模式函数共享上下文
        MeshShaderModeContext mode_ctx{};
        mode_ctx.stage_interface  = resolved_stage_interface;
        mode_ctx.max_invocations  = max_invocations;
        mode_ctx.max_vertices     = max_vertices;
        mode_ctx.varying_cfg      = &varying_cfg;
        mode_ctx.emit_world_pos   = FindMaterialStageInterfaceEntry(*resolved_stage_interface, InterStageSemantic::WorldPosition) != nullptr;
        mode_ctx.emit_world_normal = FindMaterialStageInterfaceEntry(*resolved_stage_interface, InterStageSemantic::WorldNormal) != nullptr;

        switch (mode)
        {
        case MeshShaderMode::VertexPassthrough:
            EmitVertexPassthroughBody(ms, mode_ctx, primitive_type);
            break;
        case MeshShaderMode::LineQuad:
            EmitLineQuadBody(ms, mode_ctx, position_format);
            break;
        case MeshShaderMode::CharQuad:
            EmitCharQuadBody(ms, mode_ctx);
            break;
        }

        ms += "}\n";

        return ms;
    }
}//namespace hgl::graph::mtl
