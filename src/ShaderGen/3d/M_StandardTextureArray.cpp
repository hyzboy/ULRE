#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/mtl/UBOCommon.h>
#include <hgl/shadergen/MaterialCompiler.h>
#include <hgl/shadergen/CompositorAssembler.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <hgl/common/RenderAssignDef.h>
#include <cstdio>
#include <vector>

#include "../common/MFSkyLight.h"
#include "StandardSharedSpec.h"

namespace hgl::graph::mtl{
namespace
{
    constexpr const char mi_codes[] = R"(
        uint  base_color;
        float metallic;
        float roughness;
        float normal_scale;
        uint  texture_id;
    )";
    constexpr const uint32_t mi_bytes = sizeof(uint32_t) * 2 + sizeof(float) * 3;

    constexpr FixedVertexEntry STANDARD_ARRAY_VERTEX[] = {
        { VK_FORMAT_R32G32B32_SFLOAT, VertexSemantic::Position },
        { VK_FORMAT_R32G32_SFLOAT,    VertexSemantic::TexCoord },
        { VK_FORMAT_R32G32B32_SFLOAT, VertexSemantic::Normal },
    };
}

MaterialCreateInfo *CreateStandardTextureArray(const contract::PhysicalDeviceProfileLite *profile, const Material3DCreateConfig *cfg)
{
    Material3DCreateConfig cfg_with_mi = cfg ? *cfg : Material3DCreateConfig();
    cfg_with_mi.material_instance = true;

    SkyLightAmbientModel ambient = cfg_with_mi.sky_ambient_model;

    // Adapter layer: only difference from Standard is "sampler2DArray".
    std::vector<FixedDescriptorEntry> dynamic_descriptors = BuildStandardDescriptors("sampler2DArray");

    std::vector<const char *> unused_resources;
    ApplySkyLightResourceInjection(
        GetSkyLightResourceInjectionSpec(ambient),
        dynamic_descriptors,
        unused_resources);

    FixedMaterialDef dynamic_def {
        "StandardTextureArray_v1",
        PrimitiveType::Triangles,
        STANDARD_ARRAY_VERTEX,
        uint32_t(sizeof(STANDARD_ARRAY_VERTEX) / sizeof(STANDARD_ARRAY_VERTEX[0])),
        dynamic_descriptors.data(),
        uint32_t(dynamic_descriptors.size()),
        mi_codes,
        mi_bytes,
    };

    CompositorAssembler assembler("ShaderLibrary");

    auto result = assembler.Assemble(
        SurfaceType::Standard,
        BlendMode::Opaque,
        PassType::ForwardOpaque,
        QualityTier::Medium,
        PlatformBackend::PC,
        "compositor/main_forward_lit.vert.glsl",
        "compositor/main_forward_lit.frag.glsl",
        "surface/standard_texturearray_surface.glsl"  // adapter: 2DArray surface
    );

    if (!result.success)
    {
        std::fprintf(stderr, "[StandardTextureArray] CompositorAssembler failed: %s\n",
            result.error_message.c_str());
        return nullptr;
    }

    MaterialCreateInfo *mci = CompileCompositorMaterial(
        profile,
        dynamic_def,
        result.vertex_glsl,
        result.fragment_glsl,
        &cfg_with_mi);

    if (!mci)
        std::fprintf(stderr, "[StandardTextureArray] CompileCompositorMaterial failed\n");
    return mci;
}

}//namespace hgl::graph::mtl
