#include <hgl/shadergen/MaterialStageInterface.h>

namespace hgl::graph::mtl
{
    namespace
    {
        const char *GetGLSLTypeName(
            const ShaderSemanticScalarType scalar_type,
            const uint8 component_count) noexcept
        {
            if (component_count == 0 || component_count > 4)
                return nullptr;

            switch (scalar_type)
            {
            case ShaderSemanticScalarType::Float:
            {
                static const char *const names[] =
                    {nullptr, "float", "vec2", "vec3", "vec4"};
                return names[component_count];
            }
            case ShaderSemanticScalarType::SignedInteger:
            {
                static const char *const names[] =
                    {nullptr, "int", "ivec2", "ivec3", "ivec4"};
                return names[component_count];
            }
            case ShaderSemanticScalarType::UnsignedInteger:
            {
                static const char *const names[] =
                    {nullptr, "uint", "uvec2", "uvec3", "uvec4"};
                return names[component_count];
            }
            case ShaderSemanticScalarType::Boolean:
            {
                static const char *const names[] =
                    {nullptr, "bool", "bvec2", "bvec3", "bvec4"};
                return names[component_count];
            }
            default:
                return nullptr;
            }
        }

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
        switch (error)
        {
        case MaterialStageInterfaceError::None: return "None";
        case MaterialStageInterfaceError::InvalidVaryingConfiguration: return "InvalidVaryingConfiguration";
        case MaterialStageInterfaceError::MissingSemanticMetadata: return "MissingSemanticMetadata";
        case MaterialStageInterfaceError::InvalidContract: return "InvalidContract";
        }
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
        if (varying.emit_texture_layer_id)
            add(InterStageSemantic::TextureLayerID);
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
        return mask;
    }

    bool BuildMaterialStageInterface(
        const MaterialVertexVaryingConfig &varying,
        ValueArray<InterStageSemanticContractEntry> &out_entries,
        MaterialStageInterfaceDiagnostic &out_diagnostic) noexcept
    {
        out_entries.Clear();
        out_diagnostic = {};

        if (varying.texture_layer_id_uses_data_index
         && (!varying.emit_texture_layer_id
          || !varying.emit_data_index_id))
        {
            return SetFailure(
                out_diagnostic,
                MaterialStageInterfaceError::InvalidVaryingConfiguration,
                InterStageSemantic::TextureLayerID);
        }

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
        if (entry.interpolation == InterStageInterpolation::Flat)
            out_declaration += "flat ";
        else if (entry.interpolation
            == InterStageInterpolation::NoPerspective)
            out_declaration += "noperspective ";
        out_declaration += direction;
        out_declaration += " ";
        out_declaration += type_name;
        out_declaration += " ";
        out_declaration += info->shader_symbol;
        out_declaration += ";";
        return true;
    }

    bool BuildGLSLMaterialSurfaceInput(
        const ValueArray<InterStageSemanticContractEntry> &entries,
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
            "    si.textureLayerID = 0u;\n";

        if (FindMaterialStageInterfaceEntry(
                entries, InterStageSemantic::WorldPosition))
        {
            out_code += "    si.worldPos = fragWorldPos;\n";
            out_code +=
                "    si.viewDir = normalize(-fragWorldPos);\n";
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
        if (FindMaterialStageInterfaceEntry(
                entries, InterStageSemantic::TextureLayerID))
            out_code +=
                "    si.textureLayerID = fragTextureLayerID;\n";

        out_code += "    const uint materialDataIndex = ";
        out_code += FindMaterialStageInterfaceEntry(
                entries, InterStageSemantic::DataIndexID)
            ? "fragDataIndexID;\n" : "0u;\n";
        return true;
    }
}
