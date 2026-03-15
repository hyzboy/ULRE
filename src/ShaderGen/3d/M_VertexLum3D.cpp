#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include"S_VertexLuminance3D.h"
#include<cstdio>
#include<string>

namespace hgl::graph::mtl{

MaterialCreateInfo *CreateVertexLuminance3D(const contract::PhysicalDeviceProfileLite *profile,Material3DCreateConfig *cfg)
{
    cfg->material_instance=true;

    const bool use_vec2_position = cfg && cfg->position_format.ToCode() == VAT_VEC2.ToCode();

    const FixedMaterialDef &fixed_def = use_vec2_position
        ? VERTEX_LUMINANCE_3D_DEF_VEC2
        : VERTEX_LUMINANCE_3D_DEF_VEC3;

    // 通过 CompositorAssembler 从 .glsl 模板文件组装 VS/FS
    CompositorAssembler assembler("ShaderLibrary");

    const char *vs_template = use_vec2_position
        ? "compositor/main_forward_unlit_luminance_2d.vert.glsl"
        : "compositor/main_forward_unlit_luminance.vert.glsl";

    auto result = assembler.Assemble(
        SurfaceType::Unlit,
        BlendMode::Opaque,
        PassType::ForwardOpaque,
        QualityTier::Medium,
        PlatformBackend::PC,
        vs_template,                                            // VS: Pos+Lum+TID+MIID
        "compositor/main_forward_unlit_luminance.frag.glsl",    // FS: luminance+MIID
        "surface/unlit_luminance_surface.glsl"                  // Surface: MI color × luminance
    );

    if (!result.success)
    {
        std::fprintf(stderr, "[VertexLuminance3D] CompositorAssembler failed: %s\n",
            result.error_message.c_str());
        return nullptr;
    }

    MaterialCreateInfo *mci = CompileCompositorMaterial(
        profile,
        fixed_def,
        result.vertex_glsl,
        result.fragment_glsl,
        cfg);

    if (!mci)
        std::fprintf(stderr, "[VertexLuminance3D] CompileCompositorMaterial failed\n");
    return mci;
}
}//namespace hgl::graph::mtl
