#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<cstdio>
#include<hgl/mtl/MaterialVariantDesc.h>

namespace hgl::graph::mtl{
namespace
{
    constexpr const char VERTEX_LUMINANCE_2D_MI_CODES[] = "vec4 Color;";
    constexpr const uint32_t VERTEX_LUMINANCE_2D_MI_BYTES = sizeof(hgl::math::Vector4f);

    constexpr FixedVertexEntry VERTEX_LUMINANCE_2D_VERTEX[] = {
        { VAT_VEC2, VertexInputRate::Vertex, VAN::Position },
        { VAT_FLOAT, VertexInputRate::Vertex, VAN::Luminance },
    };

    const FixedUBODescriptors VERTEX_LUMINANCE_2D_UBOS = {
        {UBODescriptorSemantic::ViewportInfo, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS)},
        {UBODescriptorSemantic::CameraInfo,   uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS)},
    };

    const FixedSSBODescriptors VERTEX_LUMINANCE_2D_SSBOS = {
        {SSBODescriptorSemantic::LocalToWorld,       uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS)},
        {SSBODescriptorSemantic::TransformID,        uint32_t(VK_SHADER_STAGE_VERTEX_BIT)},
        {SSBODescriptorSemantic::MaterialInstanceID, uint32_t(VK_SHADER_STAGE_VERTEX_BIT)},
        {SSBODescriptorSemantic::MaterialInstance,   uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS)},
    };

    const FixedMaterialDef VERTEX_LUMINANCE_2D_DEF {
        "VertexLuminance2D",
        PrimitiveType::Triangles,
        VERTEX_LUMINANCE_2D_VERTEX,
        uint32_t(sizeof(VERTEX_LUMINANCE_2D_VERTEX) / sizeof(VERTEX_LUMINANCE_2D_VERTEX[0])),
        &VERTEX_LUMINANCE_2D_UBOS,
        &VERTEX_LUMINANCE_2D_SSBOS,
        nullptr,
        VERTEX_LUMINANCE_2D_MI_CODES,
        VERTEX_LUMINANCE_2D_MI_BYTES,
    };
}

MaterialCreateInfo *CreateVertexLuminance2D(const contract::PhysicalDeviceProfileLite *profile,Material3DCreateConfig *cfg)
{
    cfg->material_instance=true;

    MaterialVariantKey var_key;
    var_key.SetVertexAttribEnabled(VertexAttrib::Luminance);
    var_key.SetVertexAttribEnabled(VertexAttrib::Position);
    const MaterialVariantDesc *var_desc = GetBuiltinVariantRegistry().QueryVariant(var_key);
    if (!var_desc)
    {
        std::fprintf(stderr, "[VertexLuminance2D] VariantRegistry lookup failed\n");
        return nullptr;
    }

    CompositorAssembler assembler("ShaderLibrary");

    auto result = assembler.Assemble(var_key, *var_desc);

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
