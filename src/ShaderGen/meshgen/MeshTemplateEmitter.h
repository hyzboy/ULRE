// MeshTemplateEmitter.h — 通用 mesh shader 生成器（调度入口）
//
// 生成 GLSL 骨架，顶点输出走 mesh 图元：
//   - 通用模式（默认）：每线程 1 顶点，直通到 gl_MeshVerticesEXT（模拟 VS 行为）
//   - Line quad 模式：每线程 1 线段，展开成 quad（2 三角形）
//
// 复用现有 s1_* 模块（LoadVertexData 读 SSBO）——通过宏把 gl_VertexIndex
// 替换为 gl_LocalInvocationIndex（mesh shader 无 gl_VertexIndex）。
//
// 输出：带来源溯源的 ShaderDocument 块（varying/descriptor/Stage1/2/3 结构），
// 由 MeshTemplateComposer 组合、BuildMaterialStageDocument 合并资源声明。

#pragma once

#include <hgl/mtl/MeshShaderMode.h>
#include <hgl/mtl/VertexShaderNodeConfig.h>
#include <hgl/mtl/MaterialStageInterface.h>
#include <hgl/mtl/ShaderDocument.h>
#include <hgl/log/Log.h>
#include <vulkan/vulkan.h>
#include <algorithm>
#include <string>
#include <vector>
#include <hgl/mtl/MaterialVertexVaryingConfig.h>

// 子模块与模式描述符
#include "MeshShaderHeaderGen.h"
#include "MeshShaderVertexAdapter.h"
#include "MeshShaderVaryingGen.h"
#include "MeshShaderModeVertexPassthrough.h"
#include "MeshShaderModeLineQuad.h"
#include "MeshShaderModeCharQuad.h"
#include "MeshModeDescriptor.h"

namespace hgl::graph::mtl
{
    // MeshTemplateEmitter — 生成 mesh stage GLSL
    //
    // 参数说明：
    //   max_invocations — threadgroup 大小（受设备 maxMeshWorkGroupSizeX 限制）
    //
    // 通用模式输出拓扑：triangle list（每 3 连续顶点 1 三角形）
    // Line 模式输出拓扑：triangle list（每线段 4 顶点 2 三角形）
    // MeshShaderMode 枚举定义见 inc/hgl/mtl/MeshShaderMode.h（MaterialDefinition 共享）

    // mesh shader GLSL 初始预留字节数（纯性能提示，防 realloc——典型 mesh 输出
    // 规模量级；内容由最终 GLSL 长度决定，无需精确）
    static constexpr uint32_t kMeshShaderInitialReserve = 3072;

    inline bool EmitMeshTemplateDocument(
        const VertexShaderNodeConfig &node_cfg,
        const MaterialVertexVaryingConfig &varying_cfg,
        VkFormat position_format,
        MeshShaderMode mode,
        uint32_t max_invocations,
        ShaderDocument &out_document,
        const ShaderDocument *resolved_input_document = nullptr,
        const ShaderDocument *provider_document = nullptr,
        const ValueArray<InterStageSemanticContractEntry>
            *resolved_stage_interface = nullptr)
    {
        // ── 获取模式描述符 ───────────────────────────────────────────────
        const MeshModeDescriptor *desc = GetMeshModeDescriptor(mode);
        if (!desc)
        {
            GLogError("[ShaderGen] Unhandled MeshShaderMode(%u)",
                      static_cast<uint32>(mode));
            return false;
        }

        // ── 拓扑与容量解析 ───────────────────────────────────────────────
        MeshModeCapacity capacity{};
        if (!desc->resolve_topology || !desc->resolve_topology(max_invocations, capacity))
            return false;

        const uint32_t max_vertices   = capacity.max_vertices;
        const uint32_t max_primitives = capacity.max_primitives;

        // ── Stage Interface ───────────────────────────────────────────────
        ValueArray<InterStageSemanticContractEntry> adapted_stage_interface;
        MaterialStageInterfaceDiagnostic stage_interface_diagnostic{};
        if (!resolved_stage_interface)
        {
            if (!BuildMaterialStageInterface(varying_cfg,
                                             adapted_stage_interface,
                                             stage_interface_diagnostic))
                return false;
            resolved_stage_interface = &adapted_stage_interface;
        }

        // ── Stage 2 (Mapping) 模块解析（独立于 Stage 3）────────────────────
        const char *stage2_module = nullptr;
        if (desc->resolve_stage2_mapping)
        {
            stage2_module = desc->resolve_stage2_mapping(node_cfg);
            if (!stage2_module)
                return false;   // 未映射的 position_mapping = 映射缺失，硬失败（编译失败）而非静默错渲
        }

        // ── Stage 3 (Projection) 模块解析（独立于 Stage 2）──────────────────
        const char *stage3_module = nullptr;
        if (desc->resolve_stage3_projection)
        {
            stage3_module = desc->resolve_stage3_projection(node_cfg);
            if (!stage3_module)
                return false;
        }

        out_document.Clear();
        struct PendingBlock
        {
            ShaderDocumentBlock block{};
            int order = 0;
            size_t sequence = 0;
        };
        std::vector<PendingBlock> pending_blocks;
        pending_blocks.reserve(16);

        const auto add_block =
            [&pending_blocks](
                const ShaderDocumentBlockKind kind,
                const std::string &text,
                const char *logical_name,
                const char *module = nullptr,
                const char *path = nullptr)
            {
                if (text.empty())
                    return;

                PendingBlock entry{};
                entry.block.kind = kind;
                entry.block.text = AnsiString(text.c_str());
                entry.block.source.stage = "mesh";
                entry.block.source.logical_name = logical_name;
                if (module)
                    entry.block.source.module = module;
                if (path)
                    entry.block.source.path = path;
                entry.order = ShaderDocument::GetBlockOrder(kind);
                entry.sequence = pending_blocks.size();
                pending_blocks.push_back(entry);
            };
        const auto append_document =
            [&pending_blocks](const ShaderDocument *document)
            {
                if (!document)
                    return;
                for (int index = 0; index < document->GetBlockCount(); ++index)
                {
                    const ShaderDocumentBlock &block = document->GetBlock(index);
                    PendingBlock entry{};
                    entry.block = block;
                    entry.block.source.stage = "mesh";
                    entry.order = ShaderDocument::GetBlockOrder(block.kind);
                    entry.sequence = pending_blocks.size();
                    pending_blocks.push_back(entry);
                }
            };

        std::string fragment;
        fragment.reserve(kMeshShaderInitialReserve);

        // 1. Version
        EmitMeshShaderVersion(fragment);
        add_block(ShaderDocumentBlockKind::Version, fragment,
                  "MeshTemplateEmitter.Version", "MeshShaderHeaderGen");

        // 2. Extensions & RootAddresses push constant
        fragment.clear();
        EmitMeshShaderExtensions(fragment);
        add_block(ShaderDocumentBlockKind::Extension, fragment,
                  "MeshTemplateEmitter.Extensions", "MeshShaderHeaderGen");

        // 3. Defines (由模式正向声明)
        if (desc->emit_defines)
        {
            fragment.clear();
            desc->emit_defines(fragment, varying_cfg);
            add_block(ShaderDocumentBlockKind::Define, fragment,
                      "MeshTemplateEmitter.Defines", desc->name);
        }

        // 4. Header Resources (UBOs 正向声明集合驱动)
        fragment.clear();
        hgl::OrderedSet<DescriptorSemantic> ubos;
        if (desc->resolve_ubos)
            desc->resolve_ubos(node_cfg, varying_cfg, ubos);

        EmitMeshShaderHeaderResources(
            fragment,
            node_cfg,
            max_invocations,
            max_vertices,
            max_primitives,
            ubos);
        add_block(ShaderDocumentBlockKind::Resource, fragment,
                  "MeshTemplateEmitter.HeaderResources", "MeshShaderHeaderGen");

        // 5. Stage 1: 顶点输入适配 (SSBO)
        fragment.clear();
        EmitVertexAdapter(fragment);
        add_block(ShaderDocumentBlockKind::Resource, fragment,
                  "MeshTemplateEmitter.VertexAdapter", "MeshShaderVertexAdapter");

        // 6. ColorPalette UBO (由 UBO 声明集判定)
        if (ubos.Contains(DescriptorSemantic::MaterialColorPalette))
        {
            fragment.clear();
            EmitColorPaletteUBO(fragment, ubos);
            add_block(ShaderDocumentBlockKind::Resource, fragment,
                      "MeshTemplateEmitter.ColorPalette", "MeshShaderHeaderGen");
        }

        // 7. 模式专属自定义资源 (例如 CharQuad SSBO 声明)
        if (desc->emit_custom_resources)
        {
            fragment.clear();
            desc->emit_custom_resources(fragment, node_cfg, varying_cfg);
            add_block(ShaderDocumentBlockKind::Resource, fragment,
                      "MeshTemplateEmitter.CustomResources", desc->name);
        }

        // 8. Varying 输出 (per-vertex 数组，mesh shader 语义契约)
        fragment.clear();
        EmitVaryingDeclarations(
            fragment, *resolved_stage_interface, max_vertices, max_primitives);
        add_block(ShaderDocumentBlockKind::Interface, fragment,
                  "MeshTemplateEmitter.Varyings", "MeshShaderVaryingGen");

        // 9. Stage 1 模块 (顶点数据读取)
        if (resolved_input_document && resolved_input_document->GetBlockCount() > 0)
        {
            append_document(resolved_input_document);
        }
        else if (desc->resolve_stage1_input)
        {
            const char *input_module = desc->resolve_stage1_input(node_cfg, position_format);
            if (input_module)
            {
                fragment.clear();
                fragment += "#include \"";
                fragment += input_module;
                fragment += "\"\n";
                add_block(ShaderDocumentBlockKind::Module, fragment,
                          "MeshTemplateEmitter.DefaultInput", "vertex-input", input_module);
            }
        }

        append_document(provider_document);

        // 10. Stage 2 模块 (Mapping)
        if (stage2_module)
        {
            fragment.clear();
            fragment += "#include \"";
            fragment += stage2_module;
            fragment += "\"\n\n";
            add_block(ShaderDocumentBlockKind::Module, fragment,
                      "MeshTemplateEmitter.Stage2", "stage2", stage2_module);
        }

        // 11. Stage 3 模块 (Projection)
        if (stage3_module)
        {
            fragment.clear();
            fragment += "#include \"";
            fragment += stage3_module;
            fragment += "\"\n\n";
            add_block(ShaderDocumentBlockKind::Module, fragment,
                      "MeshTemplateEmitter.Stage3", "stage3", stage3_module);
        }

        // 12. MainBody
        fragment.clear();
        fragment += "\nvoid main()\n{\n";
        fragment += "    draw_params = MeshDrawParamsRef(pc_root.addr_mesh_draw_params).rows[gl_DrawID];\n";
        fragment += "\n";

        MeshShaderModeContext mode_ctx{};
        mode_ctx.stage_interface  = resolved_stage_interface;
        mode_ctx.max_invocations  = max_invocations;
        mode_ctx.max_vertices     = max_vertices;
        mode_ctx.varying_cfg      = &varying_cfg;
        mode_ctx.emit_world_pos   = FindMaterialStageInterfaceEntry(*resolved_stage_interface, InterStageSemantic::WorldPosition) != nullptr;
        mode_ctx.emit_world_normal = FindMaterialStageInterfaceEntry(*resolved_stage_interface, InterStageSemantic::WorldNormal) != nullptr;

        if (desc->emit_body)
        {
            desc->emit_body(fragment, mode_ctx, position_format);
        }

        fragment += "}\n";
        add_block(ShaderDocumentBlockKind::MainBody, fragment,
                  "MeshTemplateEmitter.MainBody", "mesh-main");

        std::sort(
            pending_blocks.begin(),
            pending_blocks.end(),
            [](const PendingBlock &lhs, const PendingBlock &rhs)
            {
                if (lhs.order != rhs.order)
                    return lhs.order < rhs.order;
                return lhs.sequence < rhs.sequence;
            });

        for (const PendingBlock &entry : pending_blocks)
            out_document.Add(entry.block.kind, entry.block.text, entry.block.source);

        return true;
    }

}//namespace hgl::graph::mtl
