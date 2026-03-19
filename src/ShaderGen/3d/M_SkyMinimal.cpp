#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/shadergen/MaterialCompiler.h>
#include <hgl/shadergen/CompositorAssembler.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <hgl/common/RenderAssignDef.h>
#include <cstdio>
#include <hgl/mtl/MaterialVariantDesc.h>

namespace hgl::graph::mtl{
namespace
{
    constexpr FixedVertexEntry SKY_MINIMAL_VERTEX[] = {
        { VAT_VEC3, VertexInputGroup::Basic, VertexInputRate::Vertex, VAN::Position },
    };

    constexpr FixedDescriptorEntry SKY_MINIMAL_DESCRIPTORS[] = {
        { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo", nullptr },
        { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "camera", "CameraInfo", nullptr },
        { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "sky", "SkyInfo", nullptr },
        { DescriptorSetType::Transform, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w", "LocalToWorldData", nullptr },
        { DescriptorSetType::Transform, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_VERTEX_BIT), "tid", "TransformIDData", nullptr },
    };

    constexpr FixedMaterialDef SKY_MINIMAL_DEF {
        "SkyMinimal",
        PrimitiveType::Triangles,
        SKY_MINIMAL_VERTEX,
        uint32_t(sizeof(SKY_MINIMAL_VERTEX) / sizeof(SKY_MINIMAL_VERTEX[0])),
        SKY_MINIMAL_DESCRIPTORS,
        uint32_t(sizeof(SKY_MINIMAL_DESCRIPTORS) / sizeof(SKY_MINIMAL_DESCRIPTORS[0])),
        nullptr,
        0,
    };
}//namespace

MaterialCreateInfo *CreateSkyMinimal(const contract::PhysicalDeviceProfileLite *profile, const SkyMinimalCreateConfig *cfg)
{
    MaterialVariantKey var_key;
    var_key.surface_type = SurfaceType::Sky;
    const MaterialVariantDesc *var_desc = GetBuiltinVariantRegistry().QueryVariant(var_key);
    if (!var_desc)
    {
        std::fprintf(stderr, "[SkyMinimal] VariantRegistry lookup failed\n");
        return nullptr;
    }

    CompositorAssembler assembler("ShaderLibrary");

    auto result = assembler.Assemble(var_key, PlatformBackend::PC, *var_desc);

    if (!result.success)
    {
        std::fprintf(stderr, "[SkyMinimal] CompositorAssembler failed: %s\n",
            result.error_message.c_str());
        return nullptr;
    }

    MaterialCreateInfo *mci = CompileCompositorMaterial(
        profile,
        SKY_MINIMAL_DEF,
        result.vertex_glsl,
        result.fragment_glsl,
        cfg);

    if (!mci)
        std::fprintf(stderr, "[SkyMinimal] CompileCompositorMaterial failed\n");
    return mci;
}
}//namespace hgl::graph::mtl
