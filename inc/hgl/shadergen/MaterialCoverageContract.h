#pragma once

namespace hgl::graph::mtl {}

#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/shadergen/CanonicalShaderContract.h>
#include <string>

namespace hgl::graph::shadergen
{
    using namespace hgl::graph::mtl;
    enum class MaterialCoverageMode : uint8
    {
        None = 0,
        AlphaTest,
        Dither,
        AlphaTestDither,
        AlphaToCoverage
    };

    struct MaterialCoverageContract
    {
        MaterialCoverageMode mode = MaterialCoverageMode::None;
        float alpha_cutoff = 0.5f;
        bool requires_alpha_evaluation = false;
        InterStageSemanticMask required_semantics = 0;
        bool requires_material_data = false;
        bool requires_texture = false;
        TextureSlot texture_slot = TextureSlot::OpacityMask;
    };

    bool BuildMaterialCoverageContract(
        const mtl::MaterialDefinition &definition,
        const mtl::MaterialRecipe &recipe,
        ShaderProgramPurpose purpose,
        MaterialCoverageContract &out_contract) noexcept;

    bool ApplyDepthCoverageContract(
        const MaterialCoverageContract &coverage,
        const ValueArray<InterStageSemanticContractEntry> &stage_interface,
        const char *material_source_module,
        const char *surface_module,
        const std::string &source,
        std::string &out_source);
}
