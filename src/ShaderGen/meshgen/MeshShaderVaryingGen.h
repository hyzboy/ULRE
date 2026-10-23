// MeshShaderVaryingGen.h — Varying 声明生成
//
// 按语义生成 mesh shader per-vertex 数组型 varying 声明。

#pragma once

#include <hgl/mtl/MaterialStageInterface.h>
#include <hgl/mtl/ShaderSemanticRegistry.h>
#include <string>

namespace hgl::graph::mtl
{
    // mesh shader 的 per-vertex varying 必须是数组（按顶点索引访问）。
    // per-primitive 语义（DataIndexID/StyleID——每图元恒定一份）用 perprimitiveEXT，
    // 数组尺寸 = max_primitives（写入按图元号，FS 侧 in 同加 perprimitiveEXT）。
    // 不依赖 BuildGLSLInterStageDeclaration（它生成标量 out）——按语义直接生成数组声明。
    //
    // 名字（shader_symbol）与形状（value_shape）一律查 ShaderSemanticRegistry
    // （GetInterStageSemanticInfo / GetGLSLTypeName）——不在此手写第二份映射，
    // 漂移即 mesh/FS link 错误。
    //
    // 发射白名单 = kMeshEmittedVaryingSemantics（下）。WorldTangent/WorldBinormal
    // 不进 mesh 数组 varying，保持既有行为。
    // mesh shader 能够声明成数组 varying 的语义白名单（8 项）。
    // 声明侧与「模式可写性」校验共用这一份，避免两处白名单漂移。
    inline constexpr InterStageSemantic kMeshEmittedVaryingSemantics[] =
    {
        InterStageSemantic::DataIndexID,
        InterStageSemantic::Color,
        InterStageSemantic::UV0,
        InterStageSemantic::WorldPosition,
        InterStageSemantic::WorldNormal,
        InterStageSemantic::Luminance,
        InterStageSemantic::FragDirection,
        InterStageSemantic::StyleID,
    };

    inline bool IsMeshEmittedVaryingSemantic(InterStageSemantic semantic) noexcept
    {
        for (const InterStageSemantic whitelisted : kMeshEmittedVaryingSemantics)
        {
            if (semantic == whitelisted)
                return true;
        }
        return false;
    }

    // ── Varying 写入规则表 ──────────────────────────────────────────────
    //
    // 「模式 × 语义 → GLSL 片段」的一张声明表。它同时是两件事的唯一真源：
    //   ① 发射器要写什么 —— EmitVaryingWrites 只做筛选与拼接，没有分支逻辑；
    //   ② 该模式**能**写什么 —— IsMeshVaryingWritable 驱动 MeshTemplateEmitter
    //      的可写性硬门。
    // 二者合一，才让「声称可写」与「真的写了」不可能漂移。此前是两张分开
    // 维护的表（描述符里的语义清单 + 发射器里的 if 链），LineQuad 漏写
    // WorldPosition（#9）正是一次漂移的产物。
    //
    // 片段一律是**无条件语句**（含缩进与结尾换行，逐字节即发射文本）。条件性
    // 只体现在「进不进这张表」和 color_source 上，避免片段内部再长出一层条件
    // 判断。表内顺序就是发射顺序 —— 每个模式一张表，不跨模式共享行，
    // 这样各模式的语句顺序可以各自保持原样（CharQuad 的 UV0 在前，
    // PerVertex 的 DataIndexID 在前）。

    /// 顶点色来源筛选：同一个语义可能有两套取色路径，按材质配置二选一。
    enum class MeshVaryingColorSource : uint8_t
    {
        Any = 0,     ///< 与顶点色来源无关
        Attribute,   ///< 只在顶点色来自顶点流时写
        Palette,     ///< 只在顶点色来自调色板索引时写
    };

    struct MeshVaryingWriteRule
    {
        InterStageSemantic semantic;
        MeshVaryingColorSource color_source;
        const char *glsl;
    };

    struct MeshVaryingWriteRuleSet
    {
        const MeshVaryingWriteRule *rules;
        int count;
    };

    // PerVertex（Stride Loop，每线程摊到若干顶点）：varying 全按顶点号直写。
    // 世界位置/法线一律走 s3 模块的接口，绝不在 C++ 里重算 `l2w * pos` /
    // `mat3(l2w) * n`：
    //  - 广告牌与 2D 正交的世界位置不经过 l2w，硬编码会算错（#10）
    //  - mat3(l2w) 在非均匀缩放下不保持法线垂直（#11）
    // 模块能力门已在生成期校验，需要时接口必然存在。
    inline constexpr MeshVaryingWriteRule kMeshVaryingWriteRulesPerVertex[] =
    {
        { InterStageSemantic::DataIndexID,
          MeshVaryingColorSource::Any,
          "    fragDataIndexID[vid / 3u] = gl_InstanceIndex;\n" },
        { InterStageSemantic::Color,
          MeshVaryingColorSource::Palette,
          "    fragVertexColor[vid] = unpackUnorm4x8(color_palette.color[ColorIndex]);\n" },
        { InterStageSemantic::Color,
          MeshVaryingColorSource::Attribute,
          "    fragVertexColor[vid] = Color;\n" },
        { InterStageSemantic::UV0,
          MeshVaryingColorSource::Any,
          "    fragUV0[vid] = TexCoord;\n" },
        { InterStageSemantic::Luminance,
          MeshVaryingColorSource::Any,
          "    fragLuminance[vid] = Luminance;\n" },
        { InterStageSemantic::FragDirection,
          MeshVaryingColorSource::Any,
          "    fragDirection[vid] = normalize(Position);\n" },
        { InterStageSemantic::WorldPosition,
          MeshVaryingColorSource::Any,
          "    fragWorldPos[vid] = GetWorldPos(GetLocalPos());\n" },
        { InterStageSemantic::WorldNormal,
          MeshVaryingColorSource::Any,
          "    fragWorldNormal[vid] = GetWorldNormal(Normal);\n" },
    };
    inline constexpr int kCountOfMeshVaryingWriteRulesPerVertex =
        static_cast<int>(sizeof(kMeshVaryingWriteRulesPerVertex)
                       / sizeof(kMeshVaryingWriteRulesPerVertex[0]));

    // LineQuad：每线程 1 线段 → 4 顶点。模板只读位置/颜色/亮度 SSBO，没有
    // TexCoord、没有法线、没有 per-char style；世界位置由 from_world / to_world
    // 精确给出（quad 顶点顺序固定：0/1 = from 端两个偏移，2/3 = to 端）。
    inline constexpr MeshVaryingWriteRule kMeshVaryingWriteRulesLineQuad[] =
    {
        { InterStageSemantic::DataIndexID,
          MeshVaryingColorSource::Any,
          "    const uint data_id = gl_InstanceIndex;\n"
          "    fragDataIndexID[gl_LocalInvocationIndex * 2u + 0u] = data_id;\n"
          "    fragDataIndexID[gl_LocalInvocationIndex * 2u + 1u] = data_id;\n" },
        { InterStageSemantic::Color,
          MeshVaryingColorSource::Palette,
          "    const vec4 lcolor = unpackUnorm4x8(color_palette.color[color_index]);\n"
          "    fragVertexColor[vid + 0u] = lcolor;\n"
          "    fragVertexColor[vid + 1u] = lcolor;\n"
          "    fragVertexColor[vid + 2u] = lcolor;\n"
          "    fragVertexColor[vid + 3u] = lcolor;\n" },
        { InterStageSemantic::Color,
          MeshVaryingColorSource::Attribute,
          "#ifdef S1_COLOR_GLSL\n"
          "    const vec4 vcolor = sbo_vertex_color.data[v0];\n"
          "    fragVertexColor[vid + 0u] = vcolor;\n"
          "    fragVertexColor[vid + 1u] = vcolor;\n"
          "    fragVertexColor[vid + 2u] = vcolor;\n"
          "    fragVertexColor[vid + 3u] = vcolor;\n"
          "#endif\n" },
        { InterStageSemantic::Luminance,
          MeshVaryingColorSource::Any,
          "#ifdef S1_LUMINANCE_GLSL\n"
          "    const uint lpacked = sbo_vertex_luminance.data[v0 >> 2u];\n"
          "    const float lum = float((lpacked >> ((v0 & 3u) * 8u)) & 0xFFu) / 255.0;\n"
          "    fragLuminance[vid + 0u] = lum;\n"
          "    fragLuminance[vid + 1u] = lum;\n"
          "    fragLuminance[vid + 2u] = lum;\n"
          "    fragLuminance[vid + 3u] = lum;\n"
          "#endif\n" },
        { InterStageSemantic::WorldPosition,
          MeshVaryingColorSource::Any,
          "    fragWorldPos[vid + 0u] = from_world;\n"
          "    fragWorldPos[vid + 1u] = from_world;\n"
          "    fragWorldPos[vid + 2u] = to_world;\n"
          "    fragWorldPos[vid + 3u] = to_world;\n" },
    };
    inline constexpr int kCountOfMeshVaryingWriteRulesLineQuad =
        static_cast<int>(sizeof(kMeshVaryingWriteRulesLineQuad)
                       / sizeof(kMeshVaryingWriteRulesLineQuad[0]));

    // CharQuad：自带字形 SSBO —— UV 来自字形度量、StyleID 来自字形记录，
    // 顶点色恒为 char_color。没有亮度、没有世界空间、没有 frag direction。
    inline constexpr MeshVaryingWriteRule kMeshVaryingWriteRulesCharQuad[] =
    {
        { InterStageSemantic::UV0,
          MeshVaryingColorSource::Any,
          "    fragUV0[base_vid + 0u] = vec2(rot_tl_u, rot_tl_v);  // TL\n"
          "    fragUV0[base_vid + 1u] = vec2(rot_bl_u, rot_bl_v);  // BL\n"
          "    fragUV0[base_vid + 2u] = vec2(rot_tr_u, rot_tr_v);  // TR\n"
          "    fragUV0[base_vid + 3u] = vec2(rot_br_u, rot_br_v);  // BR\n" },
        { InterStageSemantic::Color,
          MeshVaryingColorSource::Any,
          "    for (int i = 0; i < 4; i++)\n"
          "        fragVertexColor[base_vid + uint(i)] = char_color;\n" },
        { InterStageSemantic::DataIndexID,
          MeshVaryingColorSource::Any,
          "    const uint data_id = gl_DrawID;\n"
          "    fragDataIndexID[gl_LocalInvocationIndex * 2u + 0u] = data_id;\n"
          "    fragDataIndexID[gl_LocalInvocationIndex * 2u + 1u] = data_id;\n" },
        { InterStageSemantic::StyleID,
          MeshVaryingColorSource::Any,
          "    fragStyleID[gl_LocalInvocationIndex * 2u + 0u] = style_id;\n"
          "    fragStyleID[gl_LocalInvocationIndex * 2u + 1u] = style_id;\n" },
    };
    inline constexpr int kCountOfMeshVaryingWriteRulesCharQuad =
        static_cast<int>(sizeof(kMeshVaryingWriteRulesCharQuad)
                       / sizeof(kMeshVaryingWriteRulesCharQuad[0]));

    inline void EmitVaryingDeclarations(
        std::string &ms,
        const ValueArray<InterStageSemanticContractEntry> &stage_interface,
        uint32_t max_vertices,
        uint32_t max_primitives)
    {
        for (int i = 0; i < stage_interface.GetCount(); ++i)
        {
            const auto &entry = stage_interface[i];

            if (!IsMeshEmittedVaryingSemantic(entry.semantic))
                continue;

            const InterStageSemanticInfo *info =
                GetInterStageSemanticInfo(entry.semantic);
            if (!info || !info->shader_symbol || !info->shader_symbol[0])
                continue;

            const char *type_name = GetGLSLTypeName(
                info->value_shape.scalar_type, info->value_shape.component_count);
            if (!type_name)
                continue;

            const bool per_primitive = IsPerPrimitiveInterStageSemantic(entry.semantic);
            // perprimitiveEXT 自带 flat 语义（per-primitive 数据不插值）——
            // 不能与 flat 组合（glslang syntax error），类型去 flat 前缀
            std::string effective_type;
            if (!per_primitive
             && info->interpolation == InterStageInterpolation::Flat)
            {
                effective_type = "flat ";
                effective_type += type_name;
                type_name = effective_type.c_str();
            }
            const std::string array_size_str = std::to_string(
                per_primitive ? max_primitives : max_vertices);

            ms += "layout(location=";
            ms += std::to_string(entry.location);
            ms += ") ";
            if (per_primitive)
                ms += "perprimitiveEXT ";
            ms += "out ";
            ms += type_name;
            ms += " ";
            ms += info->shader_symbol;
            ms += "[";
            ms += array_size_str;
            ms += "];\n";
        }
    }

    /// 该规则集里是否有能写 semantic 的规则（可写性硬门的判定核心）。
    inline bool IsMeshVaryingWritable(
        const MeshVaryingWriteRuleSet &rule_set,
        const InterStageSemantic semantic) noexcept
    {
        for (int i = 0; i < rule_set.count; ++i)
        {
            if (rule_set.rules[i].semantic == semantic)
                return true;
        }
        return false;
    }

    /// 按规则集拼接 varying 写入。
    ///
    /// 除 color_source 之外不存在其它条件：片段本身是无条件语句，
    /// 「写不写」完全由「材质要不要求这个语义」+「取色路径对不对」决定。
    inline void EmitVaryingWrites(
        std::string &ms,
        const ValueArray<InterStageSemanticContractEntry> &stage_interface,
        const MaterialVertexVaryingConfig &varying_cfg,
        const MeshVaryingWriteRuleSet &rule_set)
    {
        for (int i = 0; i < rule_set.count; ++i)
        {
            const MeshVaryingWriteRule &rule = rule_set.rules[i];

            if (!FindMaterialStageInterfaceEntry(stage_interface, rule.semantic))
                continue;

            if (rule.color_source == MeshVaryingColorSource::Palette
             && !varying_cfg.emit_vertex_color_from_palette)
                continue;
            if (rule.color_source == MeshVaryingColorSource::Attribute
             && varying_cfg.emit_vertex_color_from_palette)
                continue;

            ms += rule.glsl;
        }
    }
}
