#include"Build2DCommon.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<cstdio>

namespace hgl::graph::mtl{
namespace
{
    std::string BuildVS(const Material2DCreateConfig *cfg, const std::string &defs)
    {
        auto p = build2d::BuildVSPrefix(cfg, defs);

        // Color vertex input
        p.glsl += "layout(location=" + std::to_string(p.next_location++) + ") in vec4 Color;\n";

        // output
        p.glsl += "\nlayout(location=0) out vec4 fragColor;\n";

        // functions + main
        p.glsl += "\n" + build2d::GetPosition2DFunc(cfg->coordinate_system, cfg->local_to_world);
        p.glsl += "\nvoid main()\n{\n";
        p.glsl += "    fragColor = Color;\n";
        p.glsl += "    gl_Position = GetPosition2D();\n";
        p.glsl += "}\n";
        return p.glsl;
    }

    std::string BuildFS()
    {
        std::string fs = build2d::FSHeader();
        fs += "layout(location=0) in vec4 fragColor;\n\n";
        fs += "layout(location=0) out vec4 FragColor;\n\n";
        fs += "void main()\n{\n";
        fs += "    FragColor = fragColor;\n";
        fs += "}\n";
        return fs;
    }
}//namespace

MaterialCreateInfo *CreateVertexColor2D(const contract::PhysicalDeviceProfileLite *profile,const Material2DCreateConfig *cfg)
{
    // Build DEF
    auto defs = build2d::BuildDescriptorDefines(cfg, false, false);

    std::vector<FixedVertexEntry> vertices;
    build2d::PushBaseVertexEntries(vertices, cfg);
    vertices.push_back({VAT_VEC4, VertexInputGroup::Basic, VertexInputRate::Vertex, VAN::Color});

    std::vector<FixedDescriptorEntry> descriptors;
    build2d::PushBaseDescriptorEntries(descriptors, cfg);

    FixedMaterialDef def {
        "VertexColor2D",
        cfg->prim,
        vertices.data(), uint32_t(vertices.size()),
        descriptors.data(), uint32_t(descriptors.size()),
        nullptr, 0,
    };

    std::string vs = BuildVS(cfg, defs);
    std::string fs = BuildFS();

    MaterialCreateInfo *mci = CompileCompositorMaterial(profile, def, vs, fs, cfg);
    if(!mci)
        std::fprintf(stderr, "[VertexColor2D] CompileCompositorMaterial failed\n");
    return mci;
}

}//namespace hgl::graph::mtl
