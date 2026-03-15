#include"Build2DCommon.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/mtl/SamplerName.h>
#include<cstdio>

namespace hgl::graph::mtl{
namespace
{
    std::string BuildVS(const Material2DCreateConfig *cfg, const std::string &defs)
    {
        auto p = build2d::BuildVSPrefix(cfg, defs);

        p.glsl += "layout(location=" + std::to_string(p.next_location++) + ") in vec2 TexCoord;\n";
        p.glsl += "\nlayout(location=0) out vec2 fragTexCoord;\n";

        p.glsl += "\n" + build2d::GetPosition2DFunc(cfg->coordinate_system, cfg->local_to_world);
        p.glsl += "\nvoid main()\n{\n";
        p.glsl += "    fragTexCoord = TexCoord;\n";
        p.glsl += "    gl_Position = GetPosition2D();\n";
        p.glsl += "}\n";
        return p.glsl;
    }

    std::string BuildFS(const std::string &defs)
    {
        std::string fs = build2d::FSHeader(defs);
        fs += "layout(set=TEX_SET, binding=TEX_BINDING) uniform sampler2D TextureBaseColor;\n\n";
        fs += "layout(location=0) in vec2 fragTexCoord;\n\n";
        fs += "layout(location=0) out vec4 FragColor;\n\n";
        fs += "void main()\n{\n";
        fs += "    FragColor = texture(TextureBaseColor, fragTexCoord);\n";
        fs += "}\n";
        return fs;
    }
}//namespace

MaterialCreateInfo *CreateRectTexture2D(const contract::PhysicalDeviceProfileLite *profile,const mtl::Material2DCreateConfig *cfg)
{
    if(!profile||!cfg)
        return(nullptr);

    mtl::Material2DCreateConfig inner=*cfg;
    inner.prim=PrimitiveType::Triangles;
    inner.position_format=VAT_VEC2;
    inner.shader_stage_flag_bit&=~(uint32_t)ShaderStage::Geometry;

    // Build DEF
    auto defs = build2d::BuildDescriptorDefines(&inner, true, false);

    std::vector<FixedVertexEntry> vertices;
    build2d::PushBaseVertexEntries(vertices, &inner);
    vertices.push_back({VAT_VEC2, VertexInputGroup::Basic, VertexInputRate::Vertex, VAN::TexCoord});

    std::vector<FixedDescriptorEntry> descriptors;
    build2d::PushBaseDescriptorEntries(descriptors, &inner);
    descriptors.push_back({DescriptorSetType::Material, DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), SamplerName::BaseColor, nullptr, "sampler2D"});

    FixedMaterialDef def {
        "RectTexture2D",
        inner.prim,
        vertices.data(), uint32_t(vertices.size()),
        descriptors.data(), uint32_t(descriptors.size()),
        nullptr, 0,
    };

    std::string vs = BuildVS(&inner, defs);
    std::string fs = BuildFS(defs);

    MaterialCreateInfo *mci = CompileCompositorMaterial(profile, def, vs, fs, &inner);
    if(!mci)
        std::fprintf(stderr, "[RectTexture2D] CompileCompositorMaterial failed\n");
    return mci;
}
}//namespace hgl::graph::mtl
