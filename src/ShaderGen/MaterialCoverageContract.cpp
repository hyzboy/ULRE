#include <hgl/shadergen/MaterialCoverageContract.h>

#include <hgl/shadergen/MaterialStageInterface.h>
#include <hgl/util/hash/FNV1a.h>
#include <cstring>

namespace hgl::graph::mtl
{
    namespace
    {
        constexpr const char CoverageMarker[] =
            "// ULRE_COVERAGE_CONTRACT";

        bool HasSemantic(
            const ValueArray<InterStageSemanticContractEntry> &entries,
            const InterStageSemantic semantic) noexcept
        {
            return FindMaterialStageInterfaceEntry(entries, semantic)
                != nullptr;
        }

        void AppendOptionalInclude(
            std::string &code,
            const char *path)
        {
            if (!path || !path[0])
                return;
            code += "#include \"";
            code += path;
            code += "\"\n";
        }
    }

    bool BuildMaterialCoverageContract(
        const MaterialDefinition &definition,
        const MaterialRecipe &recipe,
        const ShaderProgramPurpose purpose,
        MaterialCoverageContract &out_contract) noexcept
    {
        out_contract = {};
        const ResolvedMaterialRenderState state =
            ResolveMaterialRenderState(definition, recipe);
        const bool alpha_test = state.alpha_test;
        const bool dither = state.dither;
        const bool alpha_to_coverage =
            state.pipeline_config.alpha_to_coverage;

        if (alpha_to_coverage
         && purpose == ShaderProgramPurpose::ForwardColor)
        {
            out_contract.mode = MaterialCoverageMode::AlphaToCoverage;
        }
        else if (alpha_to_coverage)
        {
            out_contract.mode = MaterialCoverageMode::Dither;
        }
        else if (alpha_test && dither)
        {
            out_contract.mode =
                MaterialCoverageMode::AlphaTestDither;
        }
        else if (alpha_test)
        {
            out_contract.mode = MaterialCoverageMode::AlphaTest;
        }
        else if (dither)
        {
            out_contract.mode = MaterialCoverageMode::Dither;
        }

        out_contract.alpha_cutoff = state.alpha_cutoff;
        out_contract.requires_alpha_evaluation =
            out_contract.mode != MaterialCoverageMode::None;

        if (out_contract.requires_alpha_evaluation)
        {
            const char *surface = definition.fragment_surface_module
                ? definition.fragment_surface_module : "";
            const auto require_semantic =
                [&out_contract](const InterStageSemantic semantic)
            {
                out_contract.required_semantics |=
                    GetInterStageSemanticMask(semantic);
            };

            if (std::strcmp(
                    surface, "surface/lit_surface.glsl") == 0)
            {
                require_semantic(InterStageSemantic::DataIndexID);
                require_semantic(InterStageSemantic::TextureLayerID);
                require_semantic(InterStageSemantic::UV0);
                out_contract.requires_texture = true;
                out_contract.texture_slot = TextureSlot::OpacityMask;
            }
            else if (std::strcmp(
                        surface,
                        "surface/unlit_texture_surface.glsl") == 0)
            {
                require_semantic(InterStageSemantic::UV0);
                out_contract.requires_texture = true;
                out_contract.texture_slot = TextureSlot::BaseColor;
            }
            else if (std::strcmp(
                        surface,
                        "surface/unlit_text_surface.glsl") == 0
                  || std::strcmp(
                        surface,
                        "surface/unlit_2darray_surface.glsl") == 0)
            {
                require_semantic(InterStageSemantic::DataIndexID);
                require_semantic(InterStageSemantic::UV0);
                out_contract.requires_material_data = true;
                out_contract.requires_texture = true;
                out_contract.texture_slot = TextureSlot::BaseColor;
            }
            else if (std::strcmp(
                        surface,
                        "surface/unlit_vertexcolor_surface.glsl") == 0)
            {
                require_semantic(InterStageSemantic::Color);
            }
            else if (std::strcmp(
                        surface,
                        "surface/unlit_color3d_surface.glsl") == 0
                  || std::strcmp(
                        surface,
                        "surface/unlit_luminance_surface.glsl") == 0)
            {
                require_semantic(InterStageSemantic::DataIndexID);
                out_contract.requires_material_data = true;
            }
            else
            {
                out_contract.required_semantics =
                    GetMaterialInterStageSemanticMask(
                        definition.vertex_varying);
                out_contract.requires_material_data =
                    definition.vertex_varying.emit_data_index_id;
                out_contract.requires_texture =
                    definition.vertex_varying.emit_uv0;
                out_contract.texture_slot = TextureSlot::BaseColor;
            }
        }
        return true;
    }

    uint64 GetMaterialCoverageContractHash(
        const MaterialCoverageContract &contract) noexcept
    {
        uint64 hash = hgl::hash::FNV1aInit<uint64>();
        const uint32 schema_version = 1;
        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, schema_version);
        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, contract.mode);
        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, contract.alpha_cutoff);
        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, contract.requires_alpha_evaluation);
        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, contract.required_semantics);
        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, contract.requires_material_data);
        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, contract.requires_texture);
        return hgl::hash::FNV1aAppendValueBytes(
            hash, contract.texture_slot);
    }

    bool ApplyDepthCoverageContract(
        const MaterialCoverageContract &coverage,
        const ValueArray<InterStageSemanticContractEntry> &stage_interface,
        const char *material_source_module,
        const char *surface_module,
        const std::string &source,
        std::string &out_source)
    {
        out_source.clear();
        const size_t marker = source.find(CoverageMarker);
        if (marker == std::string::npos)
            return false;

        std::string generated;
        if (!coverage.requires_alpha_evaluation)
        {
            generated = "void main()\n{\n}\n";
        }
        else
        {
            generated +=
                "#include \"common/surface_interface.glsl\"\n";
            generated +=
                "#include \"common/alpha_compositor.glsl\"\n";
            generated += "#define HGL_COVERAGE_ONLY 1\n";
            AppendOptionalInclude(generated, material_source_module);
            AppendOptionalInclude(generated, surface_module);
            generated += "\nvoid main()\n{\n";
            generated += "    SurfaceInput si;\n";
            generated += "    si.worldPos = vec3(0.0);\n";
            generated += "    si.worldNormal = vec3(0.0, 0.0, 1.0);\n";
            generated += "    si.uv0 = vec2(0.0);\n";
            generated += "    si.uv1 = vec2(0.0);\n";
            generated += "    si.vertexColor = vec4(1.0);\n";
            generated += "    si.viewDir = vec3(0.0, 0.0, 1.0);\n";
            generated += "    si.screenPos = gl_FragCoord.xy;\n";
            generated += "    si.luminance = 1.0;\n";
            generated += "    si.textureLayerID = 0u;\n";

            if (HasSemantic(
                    stage_interface,
                    InterStageSemantic::WorldPosition))
                generated += "    si.worldPos = fragWorldPos;\n";
            if (HasSemantic(
                    stage_interface,
                    InterStageSemantic::WorldNormal))
                generated += "    si.worldNormal = fragWorldNormal;\n";
            if (HasSemantic(
                    stage_interface,
                    InterStageSemantic::UV0))
                generated += "    si.uv0 = fragUV0;\n";
            if (HasSemantic(
                    stage_interface,
                    InterStageSemantic::Color))
                generated += "    si.vertexColor = fragVertexColor;\n";
            if (HasSemantic(
                    stage_interface,
                    InterStageSemantic::Luminance))
                generated += "    si.luminance = fragLuminance;\n";
            if (HasSemantic(
                    stage_interface,
                    InterStageSemantic::TextureLayerID))
                generated +=
                    "    si.textureLayerID = fragTextureLayerID;\n";

            generated += "    const float alpha = EvalAlpha(si, ";
            generated += HasSemantic(
                    stage_interface,
                    InterStageSemantic::DataIndexID)
                ? "fragDataIndexID" : "0u";
            generated += ");\n";
            generated += "    HGLApplyAlpha(alpha);\n";
            generated += "}\n";
        }

        out_source.reserve(
            source.size() - sizeof(CoverageMarker)
            + generated.size() + 1);
        out_source.append(source, 0, marker);
        out_source += generated;
        out_source.append(
            source,
            marker + sizeof(CoverageMarker) - 1,
            std::string::npos);
        return true;
    }
}
