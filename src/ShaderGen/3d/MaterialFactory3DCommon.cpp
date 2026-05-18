/// MaterialFactory3D.cpp — 通用 3D 工厂函数实现

#include"MaterialFactory3DCommon.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/CompositorCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/shadergen/ErrorIndicatorAssembler.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/mtl/MaterialResourceManifest.h>
#include<cstdio>

namespace hgl::graph::mtl{

MaterialCreateInfo *CreateFromFixedDef3D(
    const char *debug_tag,
    const contract::PhysicalDeviceProfileLite *profile,
    const StaticMaterialDef &def,
    const MaterialVariantKey &var_key,
    const Material3DCreateConfig *cfg,
    const MaterialVariantDesc &var_desc)
{
    if (cfg && cfg->prim != def.primitive_type)
    {
        std::fprintf(stderr,
            "[%s] Primitive mismatch: cfg->prim=%u def.primitive_type=%u (using cfg value)\n",
            debug_tag ? debug_tag : "3DFactory",
            static_cast<unsigned>(cfg->prim),
            static_cast<unsigned>(def.primitive_type));
    }

    StaticMaterialDef effective_def = def;
    if (cfg)
        effective_def.primitive_type = cfg->prim;

    // Populate vertex attribute feature bits from the actual vertex layout.
    // Builder functions (e.g. BuildForwardLitVS) derive has_uv0 / has_normal etc. from these bits.
    MaterialVariantKey assemble_key = var_key;
    PopulateVariantKeyVertexAttribBits(assemble_key, effective_def);

    const hgl::graph::CoordinateSystem2D coord_2d =
        cfg ? cfg->coord_2d : hgl::graph::CoordinateSystem2D::NDC;

    CompositorAssembler assembler;

    // Phase 5: use artifact path to collect SFM requirements alongside GLSL generation
    auto vs_artifact = assembler.AssembleVertexArtifact(assemble_key, var_desc, nullptr, coord_2d);
    if (!vs_artifact.success)
    {
        std::fprintf(stderr, "[%s] CompositorAssembler VS failed: %s\n",
            debug_tag, vs_artifact.error_message.c_str());
        return nullptr;
    }

    auto fs_artifact = assembler.AssembleFragmentArtifact(assemble_key, var_desc);
    if (!fs_artifact.success)
    {
        std::fprintf(stderr, "[%s] CompositorAssembler FS failed: %s\n",
            debug_tag, fs_artifact.error_message.c_str());
        return nullptr;
    }

    // Merge VS + FS requirement sets, then convert to manifest.
    // Priority: explicit def declarations win; SFM fills in the rest.
    vs_artifact.req_set.MergeFrom(fs_artifact.req_set);
    MaterialResourceManifest merged_manifest = MaterialResourceManifest::FromStaticDef(def);
    merged_manifest.MergeKeepFirst(vs_artifact.req_set.ToManifest());

    // Project merged manifest back into a StaticMaterialDef for downstream consumers.
    // merged_manifest outlives this scope (consumed synchronously by CompileCompositorMaterial).
    StaticMaterialDef merged_def = merged_manifest.ProjectIntoStaticDef(effective_def);

    std::string vertex_glsl   = std::move(vs_artifact.glsl);
    std::string fragment_glsl = std::move(fs_artifact.glsl);

    // Phase 2: if this variant was assembled with an ErrorIndicator FS override,
    // re-assemble the fragment shader via AssembleErrorIndicatorFS so that the
    // error_code is baked in as a compile-time constant.
    if (var_desc.fs_error_code != 0)
    {
        std::string ei_fs;
        std::string ei_err;
        AssembleErrorIndicatorFS(assemble_key, var_desc, var_desc.fs_error_code, ei_fs, ei_err);
        fragment_glsl = std::move(ei_fs);
    }

    MaterialCreateInfo *mci = CompileCompositorMaterial(
        profile,
        merged_def,
        vertex_glsl,
        fragment_glsl,
        cfg);

    if (!mci)
        std::fprintf(stderr, "[%s] CompileCompositorMaterial failed\n", debug_tag);

    return mci;
}

}//namespace hgl::graph::mtl
