#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/common/RenderAssignDef.h>
#include<cstdio>
#include<hgl/mtl/MaterialVariantDesc.h>

namespace hgl::graph::mtl{
namespace
{
    constexpr const char VERTEX_LUMINANCE_2D_MI_CODES[] = "vec4 Color;";
    constexpr const uint32_t VERTEX_LUMINANCE_2D_MI_BYTES = sizeof(hgl::math::Vector4f);

    constexpr FixedVertexEntry VERTEX_LUMINANCE_2D_VERTEX[] = {
        { VAT_VEC2, VertexInputGroup::Basic, VertexInputRate::Vertex, VAN::Position },
        { VAT_FLOAT, VertexInputGroup::Basic, VertexInputRate::Vertex, VAN::Luminance },
#if defined(HGL_TRANSFORM_ID_USE_VAB)
        { Assign::TransformID::VAT_FMT, VertexInputGroup::TransformID, VertexInputRate::Instance, Assign::TransformID::ATTRIB },
#endif
        { Assign::MaterialInstanceID::VAT_FMT, VertexInputGroup::MaterialInstanceID, VertexInputRate::Instance, Assign::MaterialInstanceID::ATTRIB },
    };

    constexpr FixedDescriptorEntry VERTEX_LUMINANCE_2D_DESCRIPTORS[] = {
        { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo", nullptr },
        { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "camera", "CameraInfo", nullptr },
        { DescriptorSetType::Transform, TransformDescriptorKind, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w", "LocalToWorldData", nullptr },
    #if !defined(HGL_TRANSFORM_ID_USE_VAB)
        { DescriptorSetType::Transform, TransformIDDescriptorKind, uint32_t(VK_SHADER_STAGE_VERTEX_BIT), "tid", "TransformIDData", nullptr },
    #endif
        { DescriptorSetType::Material, MaterialInstanceDescriptorKind, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl", "MaterialInstanceData", nullptr },
    };

    constexpr FixedMaterialDef VERTEX_LUMINANCE_2D_DEF {
        "VertexLuminance2D",
        PrimitiveType::Triangles,
        VERTEX_LUMINANCE_2D_VERTEX,
        uint32_t(sizeof(VERTEX_LUMINANCE_2D_VERTEX) / sizeof(VERTEX_LUMINANCE_2D_VERTEX[0])),
        VERTEX_LUMINANCE_2D_DESCRIPTORS,
        uint32_t(sizeof(VERTEX_LUMINANCE_2D_DESCRIPTORS) / sizeof(VERTEX_LUMINANCE_2D_DESCRIPTORS[0])),
        VERTEX_LUMINANCE_2D_MI_CODES,
        VERTEX_LUMINANCE_2D_MI_BYTES,
    };
}

MaterialCreateInfo *CreateVertexLuminance2D(const contract::PhysicalDeviceProfileLite *profile,Material3DCreateConfig *cfg)
{
    cfg->material_instance=true;

    MaterialVariantKey var_key;
    var_key.feature_bits = VF_UseVertexLum | VF_UsePos2D;
    const MaterialVariantDesc *var_desc = GetBuiltinVariantRegistry().QueryVariant(var_key);
    if (!var_desc)
    {
        std::fprintf(stderr, "[VertexLuminance2D] VariantRegistry lookup failed\n");
        return nullptr;
    }

    CompositorAssembler assembler("ShaderLibrary");

    auto result = assembler.Assemble(var_key, PlatformBackend::PC, *var_desc);

    if (!result.success)
    {
        std::fprintf(stderr, "[VertexLuminance2D] CompositorAssembler failed: %s\n",
            result.error_message.c_str());
        return nullptr;
    }

    MaterialCreateInfo *mci = CompileCompositorMaterial(
        profile,
        VERTEX_LUMINANCE_2D_DEF,
        result.vertex_glsl,
        result.fragment_glsl,
        cfg);

    if (!mci)
        std::fprintf(stderr, "[VertexLuminance2D] CompileCompositorMaterial failed\n");
    return mci;
}
}//namespace hgl::graph::mtl
