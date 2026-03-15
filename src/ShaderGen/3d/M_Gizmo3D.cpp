#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include"S_Gizmo3D.h"
#include<cstdio>
#include<string>

namespace hgl::graph::mtl{

MaterialCreateInfo *CreateGizmo3D(const contract::PhysicalDeviceProfileLite *profile,Material3DCreateConfig *cfg)
{
    // 通过 CompositorAssembler 从 .glsl 模板文件组装 VS/FS
    CompositorAssembler assembler("ShaderLibrary");

    auto result = assembler.Assemble(
        SurfaceType::Unlit,
        BlendMode::Opaque,
        PassType::ForwardOpaque,
        QualityTier::Medium,
        PlatformBackend::PC,
        "compositor/main_forward_unlit_normal.vert.glsl",   // VS: Pos+Normal+TID+MIID
        "compositor/main_forward_unlit_normal.frag.glsl",   // FS: worldPos+worldNormal+MIID + camera
        "surface/gizmo3d_surface.glsl"                      // Surface: MI color + Blinn-Phong
    );

    if (!result.success)
    {
        std::fprintf(stderr, "[Gizmo3D] CompositorAssembler failed: %s\n",
            result.error_message.c_str());
        return nullptr;
    }

    if(cfg)
        cfg->material_instance=true;

    MaterialCreateInfo *mci = CompileCompositorMaterial(
        profile,
        GIZMO_3D_DEF,
        result.vertex_glsl,
        result.fragment_glsl,
        cfg);

    if (!mci)
        std::fprintf(stderr, "[Gizmo3D] CompileCompositorMaterial failed\n");
    return mci;
}
}//namespace hgl::graph::mtl
