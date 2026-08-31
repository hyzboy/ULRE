#include <hgl/mtl/MaterialStageInterface.h>

namespace hgl::graph::mtl
{
    using namespace hgl::graph::mtl;
    namespace
    {
        bool SetFailure(
            MaterialStageInterfaceDiagnostic &diagnostic,
            const MaterialStageInterfaceError error,
            const InterStageSemantic semantic =
                InterStageSemantic::Unknown) noexcept
        {
            diagnostic.error = error;
            diagnostic.semantic = semantic;
            return false;
        }
    }

    const char *GetMaterialStageInterfaceErrorName(
        const MaterialStageInterfaceError error) noexcept
    {
#define HGL_ERROR(name) case MaterialStageInterfaceError::name: return #name;
        switch (error)
        {
            HGL_MATERIAL_STAGE_INTERFACE_ERROR_LIST
        }
#undef HGL_ERROR
        return "Unknown";
    }

    InterStageSemanticMask GetMaterialInterStageSemanticMask(
        const MaterialVertexVaryingConfig &varying) noexcept
    {
        InterStageSemanticMask mask = 0;
        const auto add = [&mask](const InterStageSemantic semantic)
        {
            mask |= GetInterStageSemanticMask(semantic);
        };

        if (varying.emit_data_index_id)
            add(InterStageSemantic::DataIndexID);
        if (varying.emit_world_pos)
            add(InterStageSemantic::WorldPosition);
        if (varying.emit_world_normal)
            add(InterStageSemantic::WorldNormal);
        if (varying.emit_uv0)
            add(InterStageSemantic::UV0);
        if (varying.emit_vertex_color
         || varying.emit_vertex_color_from_palette)
            add(InterStageSemantic::Color);
        if (varying.emit_frag_direction)
            add(InterStageSemantic::FragDirection);
        if (varying.emit_luminance)
            add(InterStageSemantic::Luminance);
        if (varying.emit_style_id)
            add(InterStageSemantic::StyleID);
        return mask;
    }

    bool BuildMaterialStageInterface(
        const MaterialVertexVaryingConfig &varying,
        ValueArray<InterStageSemanticContractEntry> &out_entries,
        MaterialStageInterfaceDiagnostic &out_diagnostic) noexcept
    {
        out_entries.Clear();
        out_diagnostic = {};

        const InterStageSemanticMask mask =
            GetMaterialInterStageSemanticMask(varying);
        for (uint32 value = 1;
             value < static_cast<uint32>(InterStageSemantic::RANGE_SIZE);
             ++value)
        {
            const InterStageSemantic semantic =
                static_cast<InterStageSemantic>(value);
            if (!(mask & GetInterStageSemanticMask(semantic)))
                continue;

            const InterStageSemanticInfo *info =
                GetInterStageSemanticInfo(semantic);
            if (!info
             || info->stable_location == InvalidShaderSemanticLocation
             || info->location_width == 0)
            {
                return SetFailure(
                    out_diagnostic,
                    MaterialStageInterfaceError::MissingSemanticMetadata,
                    semantic);
            }

            out_entries.Add(
                {
                    semantic,
                    info->value_shape.scalar_type,
                    info->interpolation,
                    info->value_shape.component_count,
                    info->location_width,
                    info->stable_location
                });
        }

        ShaderInterfaceContract validation{};
        validation.inter_stage_semantics = out_entries;
        if (!ValidateShaderInterfaceContract(validation))
            return SetFailure(
                out_diagnostic,
                MaterialStageInterfaceError::InvalidContract);
        return true;
    }

    const InterStageSemanticContractEntry *FindMaterialStageInterfaceEntry(
        const ValueArray<InterStageSemanticContractEntry> &entries,
        const InterStageSemantic semantic) noexcept
    {
        for (int i = 0; i < entries.GetCount(); ++i)
        {
            if (entries[i].semantic == semantic)
                return &entries[i];
        }
        return nullptr;
    }

    bool BuildGLSLInterStageDeclaration(
        const InterStageSemanticContractEntry &entry,
        const char *direction,
        AnsiString &out_declaration)
    {
        out_declaration.Clear();
        const InterStageSemanticInfo *info =
            GetInterStageSemanticInfo(entry.semantic);
        const char *type_name =
            GetGLSLTypeName(entry.scalar_type, entry.component_count);
        if (!info
         || !info->shader_symbol
         || !direction
         || !direction[0]
         || !type_name)
            return false;

        out_declaration =
            AnsiString("layout(location=")
            + AnsiString::numberOf(entry.location)
            + AnsiString(") ");
        // per-primitive 语义（DataIndexID/StyleID）：mesh 侧 out 是 perprimitiveEXT
        // 数组，FS 侧 in 必须同加 perprimitiveEXT（VUID-08746 接口装饰匹配；
        // 扩展 GL_EXT_mesh_shader 由 MaterialShaderCompiler 统一注入 FS）。
        // glslang 要求 uint 必须 flat 且顺序为 flat 在前（perprimitiveEXT flat 报
        // unexpected FLAT——qualifier 组合顺序坑，实测 flat perprimitiveEXT 合法）。
        if (IsPerPrimitiveInterStageSemantic(entry.semantic))
            out_declaration += "flat perprimitiveEXT in ";
        else
        {
            if (entry.interpolation == InterStageInterpolation::Flat)
                out_declaration += "flat ";
            else if (entry.interpolation
                == InterStageInterpolation::NoPerspective)
                out_declaration += "noperspective ";
            out_declaration += direction;
        }
        out_declaration += " ";
        out_declaration += type_name;
        out_declaration += " ";
        out_declaration += info->shader_symbol;
        out_declaration += ";";
        return true;
    }

    bool BuildGLSLMaterialSurfaceInput(
        const ValueArray<InterStageSemanticContractEntry> &entries,
        bool camera_ubo_available,
        AnsiString &out_code)
    {
        out_code =
            "    SurfaceInput si;\n"
            "    si.worldPos = vec3(0.0);\n"
            "    si.worldNormal = vec3(0.0, 0.0, 1.0);\n"
            "    si.uv0 = vec2(0.0);\n"
            "    si.uv1 = vec2(0.0);\n"
            "    si.vertexColor = vec4(1.0);\n"
            "    si.viewDir = vec3(0.0, 0.0, 1.0);\n"
            "    si.screenPos = gl_FragCoord.xy;\n"
            "    si.luminance = 1.0;\n"
            "    si.styleID = 0u;\n";

        if (FindMaterialStageInterfaceEntry(
                entries, InterStageSemantic::WorldPosition))
        {
            out_code += "    si.worldPos = fragWorldPos;\n";
            // viewDir 需要真实相机位置（相机绕目标轨道运动时不在原点）。
            // camera UBO 仅在启用 scene lighting 的材质里声明；
            // 未启用时 viewDir 不参与光照，退回原点假设即可。
            out_code += camera_ubo_available
                ? "    si.viewDir = normalize(camera.camera_world_pos - fragWorldPos);\n"
                : "    si.viewDir = normalize(-fragWorldPos);\n";
        }
        if (FindMaterialStageInterfaceEntry(
                entries, InterStageSemantic::WorldNormal))
            out_code +=
                "    si.worldNormal = normalize(fragWorldNormal);\n";
        if (FindMaterialStageInterfaceEntry(
                entries, InterStageSemantic::UV0))
            out_code += "    si.uv0 = fragUV0;\n";
        if (FindMaterialStageInterfaceEntry(
                entries, InterStageSemantic::Color))
            out_code += "    si.vertexColor = fragVertexColor;\n";
        if (FindMaterialStageInterfaceEntry(
                entries, InterStageSemantic::Luminance))
            out_code += "    si.luminance = fragLuminance;\n";

        out_code += "    const uint materialDataIndex = ";
        out_code += FindMaterialStageInterfaceEntry(
                entries, InterStageSemantic::DataIndexID)
            ? "fragDataIndexID;\n" : "0u;\n";
        out_code += "    si.styleID = ";
        out_code += FindMaterialStageInterfaceEntry(
                entries, InterStageSemantic::StyleID)
            ? "fragStyleID;\n" : "0u;\n";
        return true;
    }
}
