#include <hgl/shadergen/ShadowShaderContractBuilder.h>

#include <hgl/common/ShaderDescriptorDef.h>
#include <hgl/graph/geo/GeometryVertexFormat.h>
#include <hgl/shadergen/MaterialStageInterface.h>
#include <hgl/shadergen/MaterialOutputContract.h>
#include <hgl/shadergen/MaterialCoverageContract.h>
#include <hgl/shadergen/ShaderCreateInfoVertex.h>
#include <hgl/util/hash/FNV1a.h>
#include <cstdlib>

namespace hgl::graph::mtl
{
    namespace
    {
        bool SetShadowContractFailure(
            ShadowShaderContractBuildDiagnostic &diagnostic,
            const ShadowShaderContractBuildError error,
            const char *detail,
            const VertexSemantic geometry_semantic =
                VertexSemantic::Unknown,
            const InterStageSemantic inter_stage_semantic =
                InterStageSemantic::Unknown,
            const DescriptorSemantic descriptor_semantic =
                DescriptorSemantic::Unknown)
        {
            diagnostic.error = error;
            diagnostic.detail = detail ? detail : "";
            diagnostic.geometry_semantic = geometry_semantic;
            diagnostic.inter_stage_semantic = inter_stage_semantic;
            diagnostic.descriptor_semantic = descriptor_semantic;
            return false;
        }

        ShaderContractStableID HashText(const char *text) noexcept
        {
            if (!text || !text[0])
                return 0;

            return hgl::hash::FNV1aAppendBytes(
                hgl::hash::FNV1aInit<uint64>(),
                text,
                std::strlen(text));
        }

        ShaderSemanticScalarType GetScalarType(
            const VertexAttribBaseType base_type) noexcept
        {
            switch (base_type)
            {
            case VertexAttribBaseType::Bool:
                return ShaderSemanticScalarType::Boolean;
            case VertexAttribBaseType::Int:
                return ShaderSemanticScalarType::SignedInteger;
            case VertexAttribBaseType::UInt:
                return ShaderSemanticScalarType::UnsignedInteger;
            case VertexAttribBaseType::Float:
                return ShaderSemanticScalarType::Float;
            default:
                return ShaderSemanticScalarType::Unknown;
            }
        }

        int CountLocationDeclarations(
            const std::string &source,
            const char *direction)
        {
            if (!direction)
                return 0;

            const std::string marker =
                std::string(" ") + direction + " ";
            int count = 0;
            size_t line_start = 0;
            while (line_start < source.size())
            {
                const size_t line_end = source.find('\n', line_start);
                const size_t length = line_end == std::string::npos
                    ? source.size() - line_start
                    : line_end - line_start;
                const std::string line = source.substr(line_start, length);
                if (line.find("layout(location=") != std::string::npos
                 && line.find(marker) != std::string::npos)
                    ++count;

                if (line_end == std::string::npos)
                    break;
                line_start = line_end + 1;
            }
            return count;
        }

        const ShaderDescriptor *FindLegacyDescriptor(
            const MaterialDescriptorInfo &descriptor_info,
            const MaterialResourceRequirement &requirement)
        {
            if (!requirement.name || !requirement.name[0])
                return nullptr;

            switch (requirement.kind)
            {
            case DescriptorKind::UBO:
                return descriptor_info.GetUBO(requirement.name);
            case DescriptorKind::SSBO:
                return descriptor_info.GetSSBO(requirement.name);
            case DescriptorKind::Texture:
                return descriptor_info.GetTexture(requirement.name);
            case DescriptorKind::TextureSampler:
                return descriptor_info.GetTextureSampler(requirement.name);
            }
            return nullptr;
        }

        uint64 GetResourceSchemaID(
            const MaterialResourceRequirement &requirement) noexcept
        {
            if (requirement.struct_name && requirement.struct_name[0])
                return HashText(requirement.struct_name);
            if (requirement.glsl_type && requirement.glsl_type[0])
                return HashText(requirement.glsl_type);

            uint64 hash = hgl::hash::FNV1aInit<uint64>();
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, requirement.kind);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, requirement.ssbo_type);
            return hash;
        }

        ShaderStageValueType ParseStageValueType(
            const std::string &name) noexcept
        {
            if (name == "float") return ShaderStageValueType::Float;
            if (name == "vec2") return ShaderStageValueType::Vec2;
            if (name == "vec3") return ShaderStageValueType::Vec3;
            if (name == "vec4") return ShaderStageValueType::Vec4;
            if (name == "int") return ShaderStageValueType::Int;
            if (name == "uint") return ShaderStageValueType::UInt;
            if (name == "bool") return ShaderStageValueType::Bool;
            return ShaderStageValueType::Unknown;
        }

        bool ParseFragmentOutputs(
            const std::string &source,
            OutputContract &output)
        {
            size_t line_start = 0;
            while (line_start < source.size())
            {
                const size_t line_end = source.find('\n', line_start);
                const size_t length = line_end == std::string::npos
                    ? source.size() - line_start
                    : line_end - line_start;
                const std::string line = source.substr(line_start, length);
                const size_t prefix = line.find("layout(location=");
                const size_t output_token = line.find(") out ");
                if (prefix != std::string::npos
                 && output_token != std::string::npos
                 && output_token > prefix + 16)
                {
                    uint32 location = 0;
                    for (size_t i = prefix + 16; i < output_token; ++i)
                    {
                        if (line[i] < '0' || line[i] > '9')
                            return false;
                        location = location * 10
                            + static_cast<uint32>(line[i] - '0');
                    }

                    const size_t type_start = output_token + 6;
                    const size_t type_end = line.find(' ', type_start);
                    const size_t name_end = line.find(';', type_end + 1);
                    if (type_end == std::string::npos
                     || name_end == std::string::npos)
                        return false;

                    const std::string type =
                        line.substr(type_start, type_end - type_start);
                    const std::string name =
                        line.substr(type_end + 1, name_end - type_end - 1);
                    const ShaderStageValueType value_type =
                        ParseStageValueType(type);
                    if (value_type == ShaderStageValueType::Unknown
                     || name.empty())
                        return false;

                    output.attachments.Add(
                        {
                            HashText(name.c_str()),
                            value_type,
                            location,
                            1,
                            0
                        });
                }

                if (line_end == std::string::npos)
                    break;
                line_start = line_end + 1;
            }
            return true;
        }
    }

    const char *GetShadowShaderContractBuildErrorName(
        const ShadowShaderContractBuildError error) noexcept
    {
        switch (error)
        {
        case ShadowShaderContractBuildError::None: return "None";
        case ShadowShaderContractBuildError::InvalidModuleGraph: return "InvalidModuleGraph";
        case ShadowShaderContractBuildError::MissingLegacyStage: return "MissingLegacyStage";
        case ShadowShaderContractBuildError::InvalidGeometryInput: return "InvalidGeometryInput";
        case ShadowShaderContractBuildError::GeometryMismatch: return "GeometryMismatch";
        case ShadowShaderContractBuildError::MissingVaryingDeclaration: return "MissingVaryingDeclaration";
        case ShadowShaderContractBuildError::VaryingCountMismatch: return "VaryingCountMismatch";
        case ShadowShaderContractBuildError::DescriptorMismatch: return "DescriptorMismatch";
        case ShadowShaderContractBuildError::InvalidOutputDeclaration: return "InvalidOutputDeclaration";
        case ShadowShaderContractBuildError::PurposeOutputMismatch: return "PurposeOutputMismatch";
        case ShadowShaderContractBuildError::InvalidCanonicalContract: return "InvalidCanonicalContract";
        }
        return "Unknown";
    }

    bool BuildShadowShaderContracts(
        const MaterialDefinition &definition,
        const MaterialDefinitionBuildRequest &request,
        const ResolvedModuleGraph &module_graph,
        const ShaderProgramBuildSpec &legacy_build_spec,
        ShadowShaderContracts &out_contracts,
        ShadowShaderContractBuildDiagnostic &out_diagnostic)
    {
        out_contracts = {};
        out_diagnostic = {};

        if (!ValidateResolvedModuleGraph(module_graph))
            return SetShadowContractFailure(
                out_diagnostic,
                ShadowShaderContractBuildError::InvalidModuleGraph,
                "resolved module graph is invalid");

        const ShaderCreateInfoVertex *vertex =
            legacy_build_spec.GetVertexShader();
        const ShaderCreateInfo *fragment =
            legacy_build_spec.GetStageShader(ShaderStage::Fragment);
        if (!vertex || !fragment)
            return SetShadowContractFailure(
                out_diagnostic,
                ShadowShaderContractBuildError::MissingLegacyStage,
                "legacy vertex or fragment stage is missing");

        const VIAArray &inputs = vertex->GetInput();
        for (uint i = 0; i < inputs.count; ++i)
        {
            const VIA &input = inputs.items[i];
            const ShaderSemanticScalarType scalar_type =
                GetScalarType(
                    static_cast<VertexAttribBaseType>(input.basetype));
            VkFormat physical_format = GetVulkanFormat(&input);
            if (input.semantic == VertexSemantic::Unknown
             || scalar_type == ShaderSemanticScalarType::Unknown
             || input.vec_size == 0
             || physical_format == VK_FORMAT_UNDEFINED)
            {
                return SetShadowContractFailure(
                    out_diagnostic,
                    ShadowShaderContractBuildError::InvalidGeometryInput,
                    input.name,
                    input.semantic);
            }

            if (request.geometry_vertex_format)
            {
                const GeometryVertexAttributeFormat *geometry =
                    request.geometry_vertex_format->Find(input.semantic);
                if (!geometry
                 || geometry->format == VK_FORMAT_UNDEFINED
                 || geometry->vec_size != input.vec_size)
                {
                    return SetShadowContractFailure(
                        out_diagnostic,
                        ShadowShaderContractBuildError::GeometryMismatch,
                        input.name,
                        input.semantic);
                }
                physical_format = geometry->format;
            }

            out_contracts.shader_interface.geometry_semantics.Add(
                {
                    input.semantic,
                    scalar_type,
                    input.vec_size,
                    1,
                    input.location,
                    static_cast<uint32>(physical_format)
                });
        }

        MaterialCoverageContract coverage{};
        if (!BuildMaterialCoverageContract(
                definition,
                request.recipe,
                request.override_shader_program_purpose
                    ? request.shader_program_purpose
                    : GetShaderProgramPurpose(
                        definition.compositor_pass),
                coverage))
            return SetShadowContractFailure(
                out_diagnostic,
                ShadowShaderContractBuildError::
                    InvalidCanonicalContract,
                "coverage contract build failed");
        const MaterialVertexVaryingConfig effective_varying =
            ResolveMaterialVertexVaryingConfig(
                definition,
                request.override_shader_program_purpose
                    ? request.shader_program_purpose
                    : GetShaderProgramPurpose(
                        definition.compositor_pass),
                coverage);

        ValueArray<InterStageSemanticContractEntry> expected_interface;
        MaterialStageInterfaceDiagnostic interface_diagnostic{};
        if (!BuildMaterialStageInterface(
                effective_varying,
                expected_interface,
                interface_diagnostic))
        {
            return SetShadowContractFailure(
                out_diagnostic,
                ShadowShaderContractBuildError::
                    InvalidCanonicalContract,
                GetMaterialStageInterfaceErrorName(
                    interface_diagnostic.error));
        }
        const std::string &vertex_glsl = vertex->GetFinalGLSL();
        const std::string &fragment_glsl = fragment->GetFinalGLSL();
        for (int i = 0; i < expected_interface.GetCount(); ++i)
        {
            const InterStageSemanticContractEntry &entry =
                expected_interface[i];
            AnsiString vertex_declaration;
            AnsiString fragment_declaration;
            if (!BuildGLSLInterStageDeclaration(
                    entry, "out", vertex_declaration)
             || !BuildGLSLInterStageDeclaration(
                    entry, "in", fragment_declaration))
                return SetShadowContractFailure(
                    out_diagnostic,
                    ShadowShaderContractBuildError::
                        MissingVaryingDeclaration,
                    "varying declaration generation failed",
                    VertexSemantic::Unknown,
                    entry.semantic);
            if (vertex_glsl.find(vertex_declaration.c_str())
                    == std::string::npos
             || fragment_glsl.find(fragment_declaration.c_str())
                    == std::string::npos)
            {
                return SetShadowContractFailure(
                    out_diagnostic,
                    ShadowShaderContractBuildError::
                        MissingVaryingDeclaration,
                    vertex_declaration.c_str(),
                    VertexSemantic::Unknown,
                    entry.semantic);
            }
        }
        out_contracts.shader_interface.inter_stage_semantics =
            expected_interface;

        if (CountLocationDeclarations(vertex_glsl, "out")
                != expected_interface.GetCount()
         || CountLocationDeclarations(fragment_glsl, "in")
                != expected_interface.GetCount())
        {
            return SetShadowContractFailure(
                out_diagnostic,
                ShadowShaderContractBuildError::VaryingCountMismatch,
                "generated stage interface declaration count differs");
        }

        const MaterialResourceLayout &layout =
            legacy_build_spec.GetMaterialResourceLayout();
        const MaterialDescriptorInfo &descriptor_info =
            legacy_build_spec.GetDescriptorInfo();
        for (const MaterialResourceRequirement &requirement :
             layout.requirements)
        {
            const ShaderDescriptor *descriptor =
                FindLegacyDescriptor(descriptor_info, requirement);
            if (!descriptor || descriptor->stage_flag == 0)
            {
                return SetShadowContractFailure(
                    out_diagnostic,
                    ShadowShaderContractBuildError::DescriptorMismatch,
                    requirement.name,
                    VertexSemantic::Unknown,
                    InterStageSemantic::Unknown,
                    requirement.semantic);
            }

            ShaderContractStableID logical_resource_id =
                HashText(requirement.name);
            if (logical_resource_id == 0)
            {
                uint64 hash = hgl::hash::FNV1aInit<uint64>();
                hash = hgl::hash::FNV1aAppendValueBytes(
                    hash, requirement.semantic);
                hash = hgl::hash::FNV1aAppendValueBytes(
                    hash, requirement.texture_slot);
                hash = hgl::hash::FNV1aAppendValueBytes(
                    hash, requirement.data_slot);
                logical_resource_id = hash;
            }

            out_contracts.shader_interface.descriptor_requirements.Add(
                {
                    logical_resource_id,
                    GetResourceSchemaID(requirement),
                    requirement.semantic,
                    requirement.semantic_layer,
                    requirement.set_type,
                    requirement.kind,
                    requirement.texture_slot,
                    requirement.ssbo_type,
                    requirement.data_slot,
                    descriptor->stage_flag,
                    1,
                    requirement.required,
                    requirement.allow_fallback
                });
        }

        out_contracts.shader_interface.entry_points.Add(
            {ShaderStage::Vertex, HashText("main.vertex")});
        out_contracts.shader_interface.entry_points.Add(
            {ShaderStage::Fragment, HashText("main.fragment")});

        out_contracts.output.purpose =
            request.override_shader_program_purpose
                ? request.shader_program_purpose
                : GetShaderProgramPurpose(
                    definition.compositor_pass);
        if (!ParseFragmentOutputs(fragment_glsl, out_contracts.output))
            return SetShadowContractFailure(
                out_diagnostic,
                ShadowShaderContractBuildError::InvalidOutputDeclaration,
                "fragment output declaration parse failed");
        out_contracts.output.depth_only =
            out_contracts.output.purpose == ShaderProgramPurpose::DepthOnly
         || out_contracts.output.purpose == ShaderProgramPurpose::ShadowDepth;

        if (out_contracts.output.depth_only
            != out_contracts.output.attachments.IsEmpty())
        {
            return SetShadowContractFailure(
                out_diagnostic,
                ShadowShaderContractBuildError::PurposeOutputMismatch,
                "pass purpose and fragment outputs disagree");
        }

        if (!ValidateShaderInterfaceContract(
                out_contracts.shader_interface)
         || !ValidateOutputContract(out_contracts.output))
        {
            return SetShadowContractFailure(
                out_diagnostic,
                ShadowShaderContractBuildError::InvalidCanonicalContract,
                "canonical shader contract validation failed");
        }

        return true;
    }
}
