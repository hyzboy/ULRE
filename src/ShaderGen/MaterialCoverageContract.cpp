#include <hgl/mtl/MaterialCoverageContract.h>

#include <hgl/mtl/MaterialStageInterface.h>
#include <hgl/util/hash/FNV1a.h>
#include <cstring>

namespace hgl::graph::mtl
{
    using namespace hgl::graph::mtl;

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
            const char *source =
                definition.fragment_material_source_module
                    ? definition.fragment_material_source_module : "";
            const auto require_semantic =
                [&out_contract](const InterStageSemantic semantic)
            {
                out_contract.required_semantics |=
                    GetInterStageSemanticMask(semantic);
            };

            if (std::strcmp(
                    source, "material/pbr_surface_source.glsl") == 0
             || std::strcmp(
                    source,
                    "material/pbr_texturearray_source.glsl") == 0)
            {
                require_semantic(InterStageSemantic::DataIndexID);
                require_semantic(InterStageSemantic::UV0);
                out_contract.requires_texture = true;
                out_contract.texture_slot = TextureSlot::OpacityMask;
            }
            else if (std::strcmp(
                        source,
                        "material/texture_source.glsl") == 0)
            {
                require_semantic(InterStageSemantic::DataIndexID);
                require_semantic(InterStageSemantic::UV0);
                out_contract.requires_texture = true;
                out_contract.texture_slot = TextureSlot::BaseColor;
            }
            else if (std::strcmp(
                        source,
                        "material/texture_array_source.glsl") == 0)
            {
                require_semantic(InterStageSemantic::DataIndexID);
                require_semantic(InterStageSemantic::UV0);
                out_contract.requires_material_data = true;
                out_contract.requires_texture = true;
                out_contract.texture_slot = TextureSlot::BaseColor;
            }
            else if (std::strcmp(
                        source,
                        "material/vertex_color_source.glsl") == 0)
            {
                require_semantic(InterStageSemantic::Color);
            }
            else if (std::strcmp(
                        source,
                        "material/unlit_source.glsl") == 0
                  || std::strcmp(
                        source,
                        "material/luminance_source.glsl") == 0)
            {
                require_semantic(InterStageSemantic::DataIndexID);
                out_contract.requires_material_data = true;
            }
            else if (std::strcmp(
                        source,
                        "material/debug_normal_source.glsl") == 0)
            {
                // Debug source alpha is a constant.
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
}
