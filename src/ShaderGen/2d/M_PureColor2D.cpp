#include"Build2DCommon.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/math/Vector.h>
#include<cstdio>

namespace hgl::graph::mtl{
namespace
{
    constexpr const char mi_codes[]="vec4 Color;";
    constexpr const uint32_t mi_bytes=sizeof(math::Vector4f);

    // ── VS GLSL (built at runtime via Build2DCommon) ──
    std::string BuildVS(const Material2DCreateConfig *cfg, const std::string &defs)
    {
        auto p = build2d::BuildVSPrefix(cfg, defs);

        // outputs
        int out_loc = 0;
        p.glsl += "\nlayout(location=" + std::to_string(out_loc++) + ") flat out uint fragMIID;\n";

        // functions + main
        p.glsl += "\n" + build2d::GetPosition2DFunc(cfg->coordinate_system, cfg->local_to_world);
        p.glsl += "\nvoid main()\n{\n";
        p.glsl += "    fragMIID = MaterialInstanceID;\n";
        p.glsl += "    gl_Position = GetPosition2D();\n";
        p.glsl += "}\n";
        return p.glsl;
    }

    // ── FS GLSL ──
    std::string BuildFS(const std::string &defs)
    {
        std::string fs = build2d::FSHeader(defs);
        fs += "struct MaterialInstance {\n    vec4 Color;\n};\n\n";
        fs += "layout(set=MI_SET, binding=MI_BINDING) readonly buffer MaterialInstanceData {\n"
              "    MaterialInstance mi[];\n} mtl;\n\n";
        fs += "layout(location=0) flat in uint fragMIID;\n\n";
        fs += "layout(location=0) out vec4 FragColor;\n\n";
        fs += "void main()\n{\n";
        fs += "    FragColor = mtl.mi[fragMIID].Color;\n";
        fs += "}\n";
        return fs;
    }
}//namespace

MaterialCreateInfo *CreatePureColor2D(const contract::PhysicalDeviceProfileLite *profile,Material2DCreateConfig *cfg)
{
    cfg->material_instance=true;

    // Compute actual set indices (Resort compacts empty sets)
    auto defs = build2d::BuildDescriptorDefines(cfg, false, true);

    // Build DEF dynamically
    std::vector<FixedVertexEntry> vertices;
    build2d::PushBaseVertexEntries(vertices, cfg);

    std::vector<FixedDescriptorEntry> descriptors;
    build2d::PushBaseDescriptorEntries(descriptors, cfg);

    FixedMaterialDef def {
        "PureColor2D",
        cfg->prim,
        vertices.data(), uint32_t(vertices.size()),
        descriptors.data(), uint32_t(descriptors.size()),
        mi_codes, mi_bytes,
    };

    std::string vs = BuildVS(cfg, defs);
    std::string fs = BuildFS(defs);

    MaterialCreateInfo *mci = CompileCompositorMaterial(profile, def, vs, fs, cfg);
    if(!mci)
        std::fprintf(stderr, "[PureColor2D] CompileCompositorMaterial failed\n");
    return mci;
}
}//namespace hgl::graph::mtl
