// MeshShaderVaryingGen.h — Varying 声明生成
//
// 按语义生成 mesh shader per-vertex 数组型 varying 声明。

#pragma once

#include <hgl/mtl/MaterialStageInterface.h>
#include <string>

namespace hgl::graph::mtl
{
    // mesh shader 的 per-vertex varying 必须是数组（按顶点索引访问）。
    // 不依赖 BuildGLSLInterStageDeclaration（它生成标量 out）——按语义直接生成数组声明。
    inline void EmitVaryingDeclarations(
        std::string &ms,
        const ValueArray<InterStageSemanticContractEntry> &stage_interface,
        uint32_t max_vertices)
    {
        const std::string array_size_str = std::to_string(max_vertices);

        for (int i = 0; i < stage_interface.GetCount(); ++i)
        {
            const auto &entry = stage_interface[i];

            // 语义 → GLSL 类型 + 变量名
            const char *type_name = nullptr;
            const char *var_name  = nullptr;

            switch (entry.semantic)
            {
            case InterStageSemantic::DataIndexID: type_name = "flat uint"; var_name = "fragDataIndexID"; break;
            case InterStageSemantic::Color:       type_name = "vec4";       var_name = "fragVertexColor"; break;
            case InterStageSemantic::UV0:         type_name = "vec2";       var_name = "fragUV0"; break;
            case InterStageSemantic::WorldPosition: type_name = "vec3";     var_name = "fragWorldPos"; break;
            case InterStageSemantic::WorldNormal: type_name = "vec3";       var_name = "fragWorldNormal"; break;
            case InterStageSemantic::Luminance:   type_name = "float";      var_name = "fragLuminance"; break;
            case InterStageSemantic::FragDirection: type_name = "vec3";     var_name = "fragDirection"; break;
            case InterStageSemantic::StyleID:     type_name = "flat uint"; var_name = "fragStyleID"; break;
            default: continue;
            }

            if (!type_name || !var_name)
                continue;

            ms += "layout(location=";
            ms += std::to_string(entry.location);
            ms += ") out ";
            ms += type_name;
            ms += " ";
            ms += var_name;
            ms += "[";
            ms += array_size_str;
            ms += "];\n";
        }
    }
}
