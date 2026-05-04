#include"MaterialFactory2D.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/CompositorCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/shadergen/PositionProviderRegistry.h>
#include<hgl/shadergen/AttributeProviderRegistry.h>
#include<cstdio>

namespace hgl::graph::mtl{

namespace
{
    constexpr const char *kAttribFetchMacroTags[] = {
        "NORMAL",
        "TANGENT",
        "COLOR",
        "TEXCOORD0",
        "TEXCOORD1",
        "JOINTS",
        "WEIGHTS",
        "INSTANCETRANSFORM",
    };

    bool UsesVertexStreamProviders(const MaterialVariantKey &key)
    {
        if (const PositionProvider *pp = FindBuiltinProvider(key.position_provider); pp && pp->needs_ssbo)
            return true;

        for (const auto provider : key.attribute_providers)
        {
            if (provider == AttributeProviderId::None || provider == AttributeProviderId::Constant)
                continue;

            if (const AttributeProvider *ap = FindBuiltinAttribProvider(provider); ap && ap->needs_ssbo)
                return true;
        }

        return false;
    }

    std::string BuildVertexStreamsMacroDefines(const MaterialVariantKey &key)
    {
        std::string defines;
        bool has_ssbo_fetch = false;

        if (const PositionProvider *pp = FindBuiltinProvider(key.position_provider); pp && pp->needs_ssbo)
        {
            has_ssbo_fetch = true;
            defines += "#define POSITION_PROVIDER_ID ";
            defines += std::to_string(uint32_t(key.position_provider));
            defines += "\n";
            defines += "#define POSITION_SSBO_SET VERTEXSTREAMS_SET\n";
            defines += "#define POSITION_SSBO_BINDING ";
            defines += std::to_string(size_t(AttributeSemantic::BuiltinCount));
            defines += "\n";
            if (key.position_provider == PositionProviderId::SSBO_PackedVec2)
                defines += "#define POSITION_SSBO_IS_VEC2 1\n";
        }

        for (size_t i = 0; i < key.attribute_providers.size(); ++i)
        {
            const AttributeProviderId provider = key.attribute_providers[i];
            if (provider == AttributeProviderId::None || provider == AttributeProviderId::Constant)
                continue;

            const AttributeProvider *ap = FindBuiltinAttribProvider(provider);
            if (!ap || !ap->needs_ssbo)
                continue;

            has_ssbo_fetch = true;

            std::string binding_macro = "#define FETCH_";
            binding_macro += kAttribFetchMacroTags[i];
            binding_macro += "_SSBO_BINDING ";
            binding_macro += std::to_string(i);
            binding_macro += "\n";
            defines += binding_macro;

            std::string provider_macro = "#define FETCH_";
            provider_macro += kAttribFetchMacroTags[i];
            provider_macro += "_PROVIDER_ID ";
            provider_macro += std::to_string(uint32_t(provider));
            provider_macro += "\n";
            defines += provider_macro;
        }

        if (has_ssbo_fetch)
            defines = "#define GEOMETRY_FETCH_SSBO 1\n" + defines;

        return defines;
    }

    void InjectVertexStreamsMacrosInto2DPreamble(std::string &vs_preamble,
                                                  const MaterialVariantKey &key)
    {
        const std::string defines = BuildVertexStreamsMacroDefines(key);
        if (defines.empty())
            return;

        const std::string marker = "#include \"2d/get_position_2d.glsl\"\n";
        const size_t marker_pos = vs_preamble.find(marker);

        if (marker_pos != std::string::npos)
            vs_preamble.insert(marker_pos, defines);
        else
            vs_preamble += defines;
    }
}

// Shared 2D compile path for low-variance creators.
// Text2D and other highly specialized creators should keep dedicated code paths.
MaterialCreateInfo *CreateFromFixedDef2D(const char *debug_tag,
                                         const contract::PhysicalDeviceProfileLite *profile,
                                         const StaticMaterialDef &def,
                                         const MaterialVariantKey &var_key,
                                         const std::string &vs_preamble,
                                         const std::string &fs_preamble,
                                         const Material2DCreateConfig *cfg,
                                         const MaterialVariantDesc &var_desc)
{
    if(!profile||!cfg)
        return(nullptr);

    // Populate vertex attribute feature bits from the actual vertex layout.
    MaterialVariantKey assemble_key = var_key;
    for (uint32_t i = 0; i < def.vertex_entry_count; ++i)
        assemble_key.SetVertexAttribEnabled(def.vertex_entries[i].attrib);

    CompositorAssembler assembler;
    const auto result = assembler.Assemble(assemble_key, var_desc);
    if(!result.success)
    {
        std::fprintf(stderr, "[%s] CompositorAssembler failed: %s\n",
                     debug_tag ? debug_tag : "2DFactory",
                     result.error_message.c_str());
        return nullptr;
    }

    const bool needs_vertex_streams = UsesVertexStreamProviders(assemble_key);

    std::string effective_vs_preamble = vs_preamble;
    if (needs_vertex_streams)
        InjectVertexStreamsMacrosInto2DPreamble(effective_vs_preamble, assemble_key);

    const std::string vs = effective_vs_preamble + result.vertex_glsl;
    const std::string fs = fs_preamble + result.fragment_glsl;

    StaticMaterialDef effective_def = def;
    if (needs_vertex_streams && !effective_def.vertex_stream_key)
        effective_def.vertex_stream_key = &assemble_key;

    MaterialCreateInfo *mci = CompileCompositorMaterial(profile, effective_def, vs, fs, cfg);
    if(!mci)
        std::fprintf(stderr, "[%s] CompileCompositorMaterial failed\n", debug_tag ? debug_tag : "2DFactory");
    else if (needs_vertex_streams)
    {
        std::fprintf(stderr,
                     "[%s] Enabled VertexStreams bridge: position_provider=%u color_provider=%u\n",
                     debug_tag ? debug_tag : "2DFactory",
                     static_cast<unsigned>(assemble_key.position_provider),
                     static_cast<unsigned>(assemble_key.attribute_providers[size_t(AttributeSemantic::Color)]));
    }

    return mci;
}

}//namespace hgl::graph::mtl
