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
}
