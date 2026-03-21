#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<cstdio>
#include<string>
#include<hgl/mtl/MaterialVariantDesc.h>

namespace hgl::graph::mtl{
namespace
{
    constexpr FixedVertexEntry VERTEX_COLOR_3D_VERTEX[] = {
        { VAT_VEC3, VertexInputRate::Vertex, VAN::Position },
        { VAT_VEC4, VertexInputRate::Vertex, VAN::Color },
    };

        constexpr FixedDescriptorEntry VERTEX_COLOR_3D_DESCRIPTORS[] = {
        MakeFixedDescriptorEntry(DescriptorSemantic::ViewportInfo, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS)),
        MakeFixedDescriptorEntry(DescriptorSemantic::CameraInfo, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS)),
        MakeFixedDescriptorEntry(DescriptorSemantic::LocalToWorld, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS)),
        MakeFixedDescriptorEntry(DescriptorSemantic::TransformID, uint32_t(VK_SHADER_STAGE_VERTEX_BIT)),
    };

    constexpr FixedMaterialDef VERTEX_COLOR_3D_DEF {
        "VertexColor3D",
        PrimitiveType::Triangles,
        VERTEX_COLOR_3D_VERTEX,
        uint32_t(sizeof(VERTEX_COLOR_3D_VERTEX) / sizeof(VERTEX_COLOR_3D_VERTEX[0])),
        VERTEX_COLOR_3D_DESCRIPTORS,
        uint32_t(sizeof(VERTEX_COLOR_3D_DESCRIPTORS) / sizeof(VERTEX_COLOR_3D_DESCRIPTORS[0])),
        nullptr,
        0,
    };
}

MaterialCreateInfo *CreateVertexColor3D(const contract::PhysicalDeviceProfileLite *profile,const Material3DCreateConfig *cfg)
{
    MaterialVariantKey var_key;
    var_key.SetVertexAttribEnabled(VertexAttrib::Color);
    const MaterialVariantDesc *var_desc = GetBuiltinVariantRegistry().QueryVariant(var_key);
    if (!var_desc)
    {
        std::fprintf(stderr, "[VertexColor3D] VariantRegistry lookup failed\n");
        return nullptr;
    }

    CompositorAssembler assembler("ShaderLibrary");

    auto result = assembler.Assemble(var_key, *var_desc);

    if (!result.success)
    {
        std::fprintf(stderr, "[VertexColor3D] CompositorAssembler failed: %s\n",
            result.error_message.c_str());
        return nullptr;
    }

    MaterialCreateInfo *mci = CompileCompositorMaterial(
        profile,
        VERTEX_COLOR_3D_DEF,
        result.vertex_glsl,
        result.fragment_glsl,
        cfg);

    if (!mci)
        std::fprintf(stderr, "[VertexColor3D] CompileCompositorMaterial failed\n");
    return mci;
}
}//namespace hgl::graph::mtl
