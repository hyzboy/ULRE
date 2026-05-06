#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/mtl/SamplerSlot.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/mtl/MaterialVariantRegistry.h>
#include<hgl/mtl/RecipeToKey.h>
#include<hgl/shadergen/device/DeviceProfile.h>
#include<hgl/shadergen/MaterialFactory3D.h>
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<cstdio>
#include "common/MaterialLibraryBootstrap.h"
#include "common/VariantKeyOps.h"
#include "common/VariantLookupService.h"
#include "common/VariantRoutingPolicy.h"

namespace hgl::graph::mtl{

bool ValidateBuiltinMaterialVariants(const std::string &shader_library_path,
                                     std::vector<std::string> &diagnostics)
{
    return GetBuiltinVariantRegistry().ValidateBuiltinVariantTemplates(shader_library_path,diagnostics);
}

std::string GetBuiltinMaterialVariantSnapshot()
{
    return GetBuiltinVariantRegistry().DumpSnapshot();
}

MaterialLOD GetDefaultMaterialLOD()
{
    // Temporary bootstrap fallback: current runtime only exposes one built-in material
    // implementation level. Future forward / VBuffer paths may choose LOD from richer context
    // instead of using a single global default.
    return MaterialLOD::Base;
}

MaterialPreset ResolveMaterialPresetForLOD(const MaterialPreset preset,
                                           const MaterialLOD lod)
{
    return routing::ResolvePresetForLOD(preset,lod);
}

MaterialVariantKey RouteKey(MaterialPreset preset,
                            uint32 extra_attrib_bits,
                            const RuntimeKeyOverrides &ov) noexcept
{
    return routing::BuildRouteKey(preset,extra_attrib_bits,ov);
}

const char *GetMaterialPresetName(const MaterialPreset mtl_id)
{
    return routing::GetPresetName(mtl_id);
}

std::unique_ptr<MaterialCreateInfo> CreateMaterialCreateInfoOwned(const contract::PhysicalDeviceProfileLite *profile,
                                                                  const MaterialVariantKey &key,
                                                                  MaterialCreateConfig *cfg)
{
    bootstrap::EnsureMaterialLibraryBootstrap();

    if(!cfg)
    {
        std::fprintf(stderr, "[MaterialLibrary] CreateMaterialCreateInfo failed: cfg is null\n");
        return nullptr;
    }

    if(!profile)
    {
        std::fprintf(stderr,
            "[MaterialLibrary] CreateMaterialCreateInfo warning: profile is null (key_hash=%llu surface=%u geom=%u tex_mode=%u tex_bits=0x%08X sampler_bits=0x%08X va_bits=0x%08X extra_bits=0x%08X)\n",
            static_cast<unsigned long long>(key.Hash()),
            static_cast<unsigned>(key.surface_type),
            static_cast<unsigned>(key.geometry_mode),
            static_cast<unsigned>(key.GetTextureSourceMode(SamplerSlot::BaseColor)),
            key.texture_source_bits,
            key.sampler_feature_bits,
            key.vertex_attribute_feature_bits,
            key.extra_feature_bits);
    }

    routing::VariantLookupResult lookup_result{};
    if(!routing::ResolveBuiltinVariantForKey(key, lookup_result))
    {
        std::fprintf(stderr,
            "[MaterialLibrary] CreateMaterialCreateInfo failed: no registered variant (key_hash=%llu surface=%u geom=%u tex_mode=%u tex_bits=0x%08X sampler_bits=0x%08X va_bits=0x%08X extra_bits=0x%08X)\n",
            static_cast<unsigned long long>(key.Hash()),
            static_cast<unsigned>(key.surface_type),
            static_cast<unsigned>(key.geometry_mode),
            static_cast<unsigned>(key.GetTextureSourceMode(SamplerSlot::BaseColor)),
            key.texture_source_bits,
            key.sampler_feature_bits,
            key.vertex_attribute_feature_bits,
            key.extra_feature_bits);
        return nullptr;
    }

    const MaterialVariantDesc *variant_desc = lookup_result.variant_desc;
    const MaterialVariantKey &registry_lookup_key = lookup_result.lookup_key;
    const MaterialVariantKey &resolved_key = lookup_result.resolved_key;

    std::fprintf(stderr,
        "[MaterialLibrary] resolved variant=%s request={%s} lookup={%s} resolved={%s}\n",
        variant_desc->variant_name.c_str(),
        routing::FormatVariantKeyForLog(key).c_str(),
        routing::FormatVariantKeyForLog(registry_lookup_key).c_str(),
        routing::FormatVariantKeyForLog(resolved_key).c_str());

    if(!variant_desc->factory_type)
    {
        std::fprintf(stderr,
            "[MaterialLibrary] CreateMaterialCreateInfo failed: variant has no factory_type assigned (variant=%s key_hash=%llu)\n",
            variant_desc->variant_name.c_str(),
            static_cast<unsigned long long>(key.Hash()));
        return nullptr;
    }

    const MaterialPreset factory_type = *variant_desc->factory_type;

    if(auto mci = MaterialFactory3D::Create(factory_type,profile,variant_desc,key,cfg))
        return mci;

    std::fprintf(stderr,
        "[MaterialLibrary] CreateMaterialCreateInfo failed: factory dispatch failed (variant=%s factory_type=%u key_hash=%llu resolved_key_hash=%llu)\n",
        variant_desc->variant_name.c_str(),
        static_cast<unsigned>(factory_type),
        static_cast<unsigned long long>(key.Hash()),
        static_cast<unsigned long long>(resolved_key.Hash()));
    return nullptr;
}

void ApplyCreateConfigToVariantKey(MaterialVariantKey &key, const MaterialCreateConfig *cfg)
{
    routing::ApplyCreateConfigOverrides(key, cfg);
}

std::unique_ptr<MaterialCreateInfo> CreateMaterialCreateInfoOwned(const contract::PhysicalDeviceProfileLite *profile,
                                                                  const MaterialPreset mtl_id,
                                                                  MaterialCreateConfig *cfg)
{
    const MaterialLOD lod = GetDefaultMaterialLOD();
    const MaterialPreset resolved_preset = ResolveMaterialPresetForLOD(mtl_id,lod);
    const char *preset_name = GetMaterialPresetName(mtl_id);
    if(preset_name&&resolved_preset!=mtl_id)
    {
        std::printf(
            "[MaterialLibrary] Preset alias resolved preset=%u (%s) -> canonical=%u (lod=%u)\n",
            static_cast<unsigned>(mtl_id),
            preset_name,
            static_cast<unsigned>(resolved_preset),
            static_cast<unsigned>(lod));
    }

    // Route through RouteKey(): this is the single entry for variant-key construction.
    MaterialVariantKey key = RouteKey(resolved_preset);

    ApplyCreateConfigToVariantKey(key, cfg);

    if (resolved_preset == MaterialPreset::PBRColor3D)
    {
        key.lighting_model = LightingModel::PBR;

        if (auto *cfg3d = As3D(cfg))
            cfg3d->lighting_model = LightingModel::PBR;
    }

    std::fprintf(stderr,
        "[MaterialLibrary] request preset=%u resolved_preset=%u key={%s}\n",
        static_cast<unsigned>(mtl_id),
        static_cast<unsigned>(resolved_preset),
        routing::FormatVariantKeyForLog(key).c_str());

    return CreateMaterialCreateInfoOwned(profile, key, cfg);
}

}//namespace hgl::graph::mtl
