#include"Build2DCommon.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/mtl/SamplerName.h>
#include<cstdio>

namespace hgl::graph::mtl{
namespace
{
    constexpr const char mi_codes[]="uint TextColor;";
    constexpr const uint32_t mi_bytes=sizeof(uint32);

    // Text is always Ortho + no L2W → fixed VS
    std::string BuildVS(const Material2DCreateConfig *cfg, const std::string &defs)
    {
        auto p = build2d::BuildVSPrefix(cfg, defs);   // Position(ivec2) + MIID

        // TexCoord vertex input
        p.glsl += "layout(location=" + std::to_string(p.next_location++) + ") in vec2 TexCoord;\n";

        // outputs
        p.glsl += "\nlayout(location=0) flat out uint fragMIID;\n";
        p.glsl += "layout(location=1) out vec2 fragTexCoord;\n";

        // functions + main
        p.glsl += "\n" + build2d::GetPosition2DFunc(cfg->coordinate_system, cfg->local_to_world);
        p.glsl += "\nvoid main()\n{\n";
        p.glsl += "    fragMIID = MaterialInstanceID;\n";
        p.glsl += "    fragTexCoord = TexCoord;\n";
        p.glsl += "    gl_Position = GetPosition2D();\n";
        p.glsl += "}\n";
        return p.glsl;
    }

    std::string BuildFS(const std::string &defs)
    {
        std::string fs = build2d::FSHeader(defs);
        fs += "struct MaterialInstance {\n    uint TextColor;\n};\n\n";
        fs += "layout(set=TEX_SET, binding=TEX_BINDING) uniform sampler2D TextureText;\n\n";
        fs += "layout(set=MI_SET, binding=MI_BINDING) readonly buffer MaterialInstanceData {\n"
              "    MaterialInstance mi[];\n} mtl;\n\n";
        fs += "layout(location=0) flat in uint fragMIID;\n";
        fs += "layout(location=1) in vec2 fragTexCoord;\n\n";
        fs += "layout(location=0) out vec4 FragColor;\n\n";
        fs += "void main()\n{\n";
        fs += "    MaterialInstance mi = mtl.mi[fragMIID];\n";
        fs += "    vec4 TextColor = unpackUnorm4x8(mi.TextColor);\n";
        fs += "    float lum = texture(TextureText, fragTexCoord).r;\n";
        fs += "    FragColor = vec4(TextColor.rgb * lum, TextColor.a);\n";
        fs += "}\n";
        return fs;
    }
}//namespace

MaterialCreateInfo *CreateText2D(const contract::PhysicalDeviceProfileLite *profile,const Text2DMaterialCreateConfig *cfg)
{
    if(!profile||!cfg)
        return(nullptr);

    Text2DMaterialCreateConfig new_cfg=*cfg;
    new_cfg.prim=PrimitiveType::Triangles;
    new_cfg.position_format=VAT_IVEC2;
    new_cfg.shader_stage_flag_bit&=~(uint32_t)ShaderStage::Geometry;

    // Build DEF
    auto defs = build2d::BuildDescriptorDefines(&new_cfg, true, true);

    std::vector<FixedVertexEntry> vertices;
    build2d::PushBaseVertexEntries(vertices, &new_cfg);
    vertices.push_back({VAT_VEC2, VertexInputGroup::Basic, VertexInputRate::Vertex, VAN::TexCoord});

    std::vector<FixedDescriptorEntry> descriptors;
    build2d::PushBaseDescriptorEntries(descriptors, &new_cfg);
    descriptors.push_back({DescriptorSetType::Material, DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), SamplerName::Text, nullptr, "sampler2D"});

    FixedMaterialDef def {
        "Text2D",
        new_cfg.prim,
        vertices.data(), uint32_t(vertices.size()),
        descriptors.data(), uint32_t(descriptors.size()),
        mi_codes, mi_bytes,
    };

    std::string vs = BuildVS(&new_cfg, defs);
    std::string fs = BuildFS(defs);

    MaterialCreateInfo *mci = CompileCompositorMaterial(profile, def, vs, fs, &new_cfg);
    if(!mci)
        std::fprintf(stderr, "[Text2D] CompileCompositorMaterial failed\n");
    return mci;
}
}//namespace hgl::graph::mtl
