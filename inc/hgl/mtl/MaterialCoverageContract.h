#pragma once

namespace hgl::graph::mtl {}

#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/mtl/CanonicalShaderContract.h>
#include <string>

namespace hgl::graph::mtl
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
}
