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
    // 漂移即 mesh/FS link 错误。发射白名单显式列出（WorldTangent/WorldBinormal
    // 不进 mesh 数组 varying，保持既有行为）。
    inline void EmitVaryingDeclarations(
        std::string &ms,
        const ValueArray<InterStageSemanticContractEntry> &stage_interface,
        uint32_t max_vertices,
        uint32_t max_primitives)
    {
        static constexpr InterStageSemantic kEmittedSemantics[] =
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

        for (int i = 0; i < stage_interface.GetCount(); ++i)
        {
            const auto &entry = stage_interface[i];

            bool in_whitelist = false;
            for (const InterStageSemantic whitelisted : kEmittedSemantics)
            {
                if (entry.semantic == whitelisted)
                {
                    in_whitelist = true;
                    break;
                }
            }
            if (!in_whitelist)
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

    enum class MeshVaryingIndexModel : uint8_t
    {
        PerVertex = 0,
        LineQuad,
        CharQuad
    };

    inline void EmitVaryingWrites(
        std::string &ms,
        const ValueArray<InterStageSemanticContractEntry> &stage_interface,
        const MaterialVertexVaryingConfig &varying_cfg,
        const MeshVaryingIndexModel model,
        const bool emit_world_pos = false,
        const bool emit_world_normal = false)
    {
        switch (model)
        {
        case MeshVaryingIndexModel::PerVertex:
            if (FindMaterialStageInterfaceEntry(stage_interface, InterStageSemantic::DataIndexID))
            {
                ms += "    fragDataIndexID[vid / 3u] = gl_InstanceIndex;\n";
            }
            if (varying_cfg.emit_vertex_color_from_palette)
                ms += "    fragVertexColor[vid] = unpackUnorm4x8(color_palette.color[ColorIndex]);\n";
            else if (FindMaterialStageInterfaceEntry(stage_interface, InterStageSemantic::Color))
                ms += "    fragVertexColor[vid] = Color;\n";

            if (FindMaterialStageInterfaceEntry(stage_interface, InterStageSemantic::UV0))
                ms += "    fragUV0[vid] = TexCoord;\n";
            if (FindMaterialStageInterfaceEntry(stage_interface, InterStageSemantic::Luminance))
                ms += "    fragLuminance[vid] = Luminance;\n";
            if (FindMaterialStageInterfaceEntry(stage_interface, InterStageSemantic::FragDirection))
                ms += "    fragDirection[vid] = normalize(Position);\n";

            if (emit_world_pos || emit_world_normal)
            {
                ms += "    mat4 _l2w = GetL2W();\n";
                ms += "    vec4 _world_pos = _l2w * GetLocalPos();\n";
                if (emit_world_pos)
                    ms += "    fragWorldPos[vid] = _world_pos.xyz;\n";
                if (emit_world_normal)
                    ms += "    fragWorldNormal[vid] = normalize(mat3(_l2w) * Normal);\n";
                ms += "    gl_MeshVerticesEXT[vid].gl_Position = camera.vp * _world_pos;\n";
            }
            else
            {
                ms += "    gl_MeshVerticesEXT[vid].gl_Position = GetClipPos(GetLocalPos());\n";
            }
            break;

        case MeshVaryingIndexModel::LineQuad:
            if (FindMaterialStageInterfaceEntry(stage_interface, InterStageSemantic::DataIndexID))
            {
                ms += "    const uint data_id = gl_InstanceIndex;\n";
                ms += "    fragDataIndexID[gl_LocalInvocationIndex * 2u + 0u] = data_id;\n";
                ms += "    fragDataIndexID[gl_LocalInvocationIndex * 2u + 1u] = data_id;\n";
            }
            if (varying_cfg.emit_vertex_color_from_palette)
            {
                ms += "    const vec4 lcolor = unpackUnorm4x8(color_palette.color[color_index]);\n";
                ms += "    fragVertexColor[vid + 0u] = lcolor;\n";
                ms += "    fragVertexColor[vid + 1u] = lcolor;\n";
                ms += "    fragVertexColor[vid + 2u] = lcolor;\n";
                ms += "    fragVertexColor[vid + 3u] = lcolor;\n";
            }
            else if (FindMaterialStageInterfaceEntry(stage_interface, InterStageSemantic::Color))
            {
                ms += "#ifdef S1_COLOR_GLSL\n";
                ms += "    const vec4 vcolor = sbo_vertex_color.data[v0];\n";
                ms += "    fragVertexColor[vid + 0u] = vcolor;\n";
                ms += "    fragVertexColor[vid + 1u] = vcolor;\n";
                ms += "    fragVertexColor[vid + 2u] = vcolor;\n";
                ms += "    fragVertexColor[vid + 3u] = vcolor;\n";
                ms += "#endif\n";
            }
            if (FindMaterialStageInterfaceEntry(stage_interface, InterStageSemantic::Luminance))
            {
                ms += "#ifdef S1_LUMINANCE_GLSL\n";
                ms += "    const uint lpacked = sbo_vertex_luminance.data[v0 >> 2u];\n";
                ms += "    const float lum = float((lpacked >> ((v0 & 3u) * 8u)) & 0xFFu) / 255.0;\n";
                ms += "    fragLuminance[vid + 0u] = lum;\n";
                ms += "    fragLuminance[vid + 1u] = lum;\n";
                ms += "    fragLuminance[vid + 2u] = lum;\n";
                ms += "    fragLuminance[vid + 3u] = lum;\n";
                ms += "#endif\n";
            }
            break;

        case MeshVaryingIndexModel::CharQuad:
            if (FindMaterialStageInterfaceEntry(stage_interface, InterStageSemantic::UV0))
            {
                ms += "    fragUV0[base_vid + 0u] = vec2(rot_tl_u, rot_tl_v);  // TL\n";
                ms += "    fragUV0[base_vid + 1u] = vec2(rot_bl_u, rot_bl_v);  // BL\n";
                ms += "    fragUV0[base_vid + 2u] = vec2(rot_tr_u, rot_tr_v);  // TR\n";
                ms += "    fragUV0[base_vid + 3u] = vec2(rot_br_u, rot_br_v);  // BR\n";
            }
            if (FindMaterialStageInterfaceEntry(stage_interface, InterStageSemantic::Color))
            {
                ms += "    for (int i = 0; i < 4; i++)\n";
                ms += "        fragVertexColor[base_vid + uint(i)] = char_color;\n";
            }
            if (FindMaterialStageInterfaceEntry(stage_interface, InterStageSemantic::DataIndexID))
            {
                ms += "    const uint data_id = gl_DrawID;\n";
                ms += "    fragDataIndexID[gl_LocalInvocationIndex * 2u + 0u] = data_id;\n";
                ms += "    fragDataIndexID[gl_LocalInvocationIndex * 2u + 1u] = data_id;\n";
            }
            if (FindMaterialStageInterfaceEntry(stage_interface, InterStageSemantic::StyleID))
            {
                ms += "    fragStyleID[gl_LocalInvocationIndex * 2u + 0u] = style_id;\n";
                ms += "    fragStyleID[gl_LocalInvocationIndex * 2u + 1u] = style_id;\n";
            }
            break;
        }
    }
}
