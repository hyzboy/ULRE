/// MaterialFactory3D.cpp — 通用 3D 工厂函数实现

#include"MaterialFactory3DCommon.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/CompositorCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<cstdio>

namespace hgl::graph::mtl{

namespace
{
    bool UsesVertexStreamProviders(const MaterialVariantKey &key)
    {
        if (key.position_provider != PositionProviderId::DirectVec3)
            return true;

        for (const auto provider : key.attribute_providers)
        {
            if (provider != AttributeProviderId::None)
                return true;
        }

        return false;
    }
}

MaterialCreateInfo *CreateFromFixedDef3D(
    const char *debug_tag,
    const contract::PhysicalDeviceProfileLite *profile,
    const StaticMaterialDef &def,
    const MaterialVariantKey &var_key,
    const Material3DCreateConfig *cfg,
    const MaterialVariantDesc &var_desc)
{
    StaticMaterialDef effective_def = def;

    if (cfg && cfg->prim != effective_def.primitive_type)
    {
        std::fprintf(stderr,
            "[%s] Primitive override: cfg->prim=%u def.primitive_type=%u (using cfg value)\n",
            debug_tag ? debug_tag : "3DFactory",
            static_cast<unsigned>(cfg->prim),
            static_cast<unsigned>(effective_def.primitive_type));

        // Respect recipe/runtime primitive overrides (e.g. PureColor3D wireframe lines).
        effective_def.primitive_type = cfg->prim;
    }

    // Populate vertex attribute feature bits from the actual vertex layout.
    // Builder functions (e.g. BuildForwardLitVS) derive has_uv0 / has_normal etc. from these bits.
    MaterialVariantKey assemble_key = var_key;
    PopulateVariantKeyVertexAttribBits(assemble_key, effective_def);

    CompositorAssembler assembler;

    auto result = assembler.Assemble(assemble_key, var_desc);

    if (!result.success)
    {
        std::fprintf(stderr, "[%s] CompositorAssembler failed: %s\n",
            debug_tag, result.error_message.c_str());
        return nullptr;
    }

    const bool needs_vertex_streams = UsesVertexStreamProviders(assemble_key);
    if (needs_vertex_streams && !effective_def.vertex_stream_key)
        effective_def.vertex_stream_key = &assemble_key;

    MaterialCreateInfo *mci = CompileCompositorMaterial(
        profile,
        effective_def,
        result.vertex_glsl,
        result.fragment_glsl,
        cfg);

    if (!mci)
        std::fprintf(stderr, "[%s] CompileCompositorMaterial failed\n", debug_tag);
    else if (needs_vertex_streams)
    {
        std::fprintf(stderr,
                     "[%s] Enabled VertexStreams bridge: position_provider=%u color_provider=%u normal_provider=%u\n",
                     debug_tag,
                     static_cast<unsigned>(assemble_key.position_provider),
                     static_cast<unsigned>(assemble_key.attribute_providers[size_t(VertexAttrib::Color)]),
                     static_cast<unsigned>(assemble_key.attribute_providers[size_t(VertexAttrib::Normal)]));
    }

    return mci;
}

}//namespace hgl::graph::mtl
