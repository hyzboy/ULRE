#include <hgl/shadergen/ShaderRequireScanner.h>
#include <hgl/mtl/DescriptorBindingContract.h>
#include <hgl/shadergen/MaterialCompiler.h>
#include "GLSLCompiler.h"

#include "SPVParseData.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <vector>

namespace hgl::graph::mtl
{
namespace
{
    struct ReflectedResource
    {
        std::string name;
        uint32_t descriptor_type = 0;
        uint32_t stage_flags = 0;
        uint32_t set = 0;
        uint32_t binding = 0;
    };

    static std::string ToUpperASCII(const std::string &text)
    {
        std::string out;
        out.reserve(text.size());

        for (const unsigned char c : text)
        {
            if (c >= 'a' && c <= 'z')
                out.push_back(char(c - ('a' - 'A')));
            else
                out.push_back(char(c));
        }

        return out;
    }

    static bool EqualsNoCase(const std::string &a, const std::string &b)
    {
        return ToUpperASCII(a) == ToUpperASCII(b);
    }

    static const char *ToTextureChannelHintName(const TextureChannelHint hint)
    {
        switch (hint)
        {
        case TextureChannelHint::RGBA: return "RGBA";
        case TextureChannelHint::Grayscale: return "Grayscale";
        default: return "Unknown";
        }
    }

    static void DumpCollectedRequirements(const ShaderAutoRequirements &requirements)
    {
        if (requirements.ubos.empty())
        {
            std::fprintf(stderr, "[ShaderRequireScanner]   UBO: (none)\n");
        }
        else
        {
            for (const auto semantic : requirements.ubos)
            {
                std::fprintf(stderr,
                             "[ShaderRequireScanner]   UBO: semantic=%s\n",
                             GetUBODescriptorSemanticName(semantic));
            }
        }

        if (requirements.ssbos.empty())
        {
            std::fprintf(stderr, "[ShaderRequireScanner]   SSBO: (none)\n");
        }
        else
        {
            for (const auto semantic : requirements.ssbos)
            {
                std::fprintf(stderr,
                             "[ShaderRequireScanner]   SSBO: semantic=%s\n",
                             GetSSBODescriptorSemanticName(semantic));
            }
        }

        if (requirements.samplers.empty())
        {
            std::fprintf(stderr, "[ShaderRequireScanner]   TEX: (none)\n");
        }
        else
        {
            for (const auto &[slot, sampler] : requirements.samplers)
            {
                std::fprintf(stderr,
                             "[ShaderRequireScanner]   TEX: slot=%s, type=%s, channel=%s\n",
                             SamplerSlotNameList[size_t(slot)],
                             GetSamplerTypeName(sampler.sampler_type),
                             ToTextureChannelHintName(sampler.channel_hint));
            }
        }
    }

    static bool TryMapReflectedUBO(const std::string &name, UBODescriptorSemantic &semantic)
    {
        for (size_t i = 0; i < UBODescriptorSemanticCount; ++i)
        {
            const UBODescriptorSemantic candidate = static_cast<UBODescriptorSemantic>(i);
            if (!IsBuiltinDescriptorSemantic(candidate))
                continue;

            const DescriptorSemanticMeta &meta = GetDescriptorSemanticMeta(candidate);
            if ((meta.name && EqualsNoCase(name, meta.name))
             || EqualsNoCase(name, GetUBODescriptorSemanticName(candidate)))
            {
                semantic = candidate;
                return true;
            }
        }

        return false;
    }

    static bool TryMapReflectedSSBO(const std::string &name, SSBODescriptorSemantic &semantic)
    {
        for (size_t i = 0; i < SSBODescriptorSemanticCount; ++i)
        {
            const SSBODescriptorSemantic candidate = static_cast<SSBODescriptorSemantic>(i);
            if (!IsBuiltinDescriptorSemantic(candidate))
                continue;

            const DescriptorSemanticMeta &meta = GetDescriptorSemanticMeta(candidate);
            if ((meta.name && EqualsNoCase(name, meta.name))
             || EqualsNoCase(name, GetSSBODescriptorSemanticName(candidate)))
            {
                semantic = candidate;
                return true;
            }
        }

        return false;
    }

    static bool TryMapReflectedSampler(const std::string &name, SamplerSlot &slot)
    {
        for (size_t i = 0; i < SamplerSlotCount; ++i)
        {
            const SamplerSlot candidate = static_cast<SamplerSlot>(i);
            if (EqualsNoCase(name, SamplerSlotNameList[i])
             || EqualsNoCase(name, ToDescriptorName(candidate))
             || EqualsNoCase(name, ToGLSLSamplerSymbol(candidate)))
            {
                slot = candidate;
                return true;
            }
        }

        return false;
    }

    static const FixedTextureSamplerDescriptor *FindBaseSamplerDescriptor(const FixedMaterialDef &base_def,
                                                                          const SamplerSlot slot)
    {
        if (!base_def.texture_samplers)
            return nullptr;

        auto iter = base_def.texture_samplers->find(slot);
        if (iter == base_def.texture_samplers->end())
            return nullptr;

        return &iter->second;
    }

    static void BuildReflectionSeedDescriptors(const FixedUBODescriptors *source,
                                               FixedUBODescriptors &out_descriptors)
    {
        if (source)
            out_descriptors = *source;
        else
            out_descriptors.clear();
    }

    static void BuildReflectionSeedDescriptors(const FixedSSBODescriptors *source,
                                               FixedSSBODescriptors &out_descriptors)
    {
        if (source)
            out_descriptors = *source;
        else
            out_descriptors.clear();
    }

    static void BuildReflectionSeedDescriptors(const FixedTextureSamplerDescriptors *source,
                                               FixedTextureSamplerDescriptors &out_descriptors)
    {
        out_descriptors.clear();

        if (!source)
            return;

        out_descriptors = *source;
    }

    static void AppendReflectedStageResources(const uint32_t descriptor_type,
                                              const uint32_t stage_flags,
                                              const ShaderResourceData<Descriptor> &bucket,
                                              std::vector<ReflectedResource> &out_resources)
    {
        for (uint32_t i = 0; i < bucket.count; ++i)
        {
            const Descriptor &item = bucket.items[i];

            ReflectedResource resource;
            resource.name = item.name;
            resource.descriptor_type = descriptor_type;
            resource.stage_flags = stage_flags;
            resource.set = item.set;
            resource.binding = item.binding;
            out_resources.emplace_back(std::move(resource));
        }
    }

    static bool CollectReflectedStageResources(const uint32_t stage_flags,
                                               const std::string &source,
                                               std::vector<ReflectedResource> &out_resources,
                                               std::string *diagnostics)
    {
        if (source.empty())
            return true;

        SPVData *spv = CompileShader(stage_flags, source.c_str());
        if (!spv)
        {
            if (diagnostics)
                *diagnostics += "CompileShader() failed for stage " + std::to_string(stage_flags) + "\n";
            return false;
        }

        SPVParseData *parse_data = ParseShaderSPV(spv);
        if (!parse_data)
        {
            if (diagnostics)
                *diagnostics += "ParseShaderSPV() returned null for stage " + std::to_string(stage_flags) + "\n";
            FreeSPVData(spv);
            return false;
        }

        for (uint32_t descriptor_type = 0; descriptor_type < VK_DESCRIPTOR_TYPE_COUNT; ++descriptor_type)
            AppendReflectedStageResources(descriptor_type, stage_flags, parse_data->resource[descriptor_type], out_resources);

        FreeShaderSPVParseData(parse_data);
        FreeSPVData(spv);
        return true;
    }

    static bool BuildReflectedRequirements(const FixedMaterialDef &base_def,
                                           const std::vector<ReflectedResource> &resources,
                                           ShaderAutoRequirements &out_requirements,
                                           std::string *diagnostics)
    {
        for (const ReflectedResource &resource : resources)
        {
            if (resource.name.empty())
                continue;

            if (resource.descriptor_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
             || resource.descriptor_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
            {
                UBODescriptorSemantic semantic = UBODescriptorSemantic::Unknown;
                if (!TryMapReflectedUBO(resource.name, semantic))
                {
                    if (diagnostics)
                        *diagnostics += "Unknown reflected UBO: " + resource.name + "\n";
                    return false;
                }

                AddFixedUBODescriptor(out_requirements.ubos, semantic, resource.stage_flags);
                continue;
            }

            if (resource.descriptor_type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
             || resource.descriptor_type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
            {
                SSBODescriptorSemantic semantic = SSBODescriptorSemantic::Unknown;
                if (!TryMapReflectedSSBO(resource.name, semantic))
                {
                    if (diagnostics)
                        *diagnostics += "Unknown reflected SSBO: " + resource.name + "\n";
                    return false;
                }

                AddFixedSSBODescriptor(out_requirements.ssbos, semantic, resource.stage_flags);
                continue;
            }

            if (resource.descriptor_type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
             || resource.descriptor_type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
             || resource.descriptor_type == VK_DESCRIPTOR_TYPE_SAMPLER)
            {
                SamplerSlot slot = SamplerSlot::BaseColor;
                if (!TryMapReflectedSampler(resource.name, slot))
                {
                    if (diagnostics)
                        *diagnostics += "Unknown reflected sampler: " + resource.name + "\n";
                    return false;
                }

                const FixedTextureSamplerDescriptor *base_sampler = FindBaseSamplerDescriptor(base_def, slot);
                if (!base_sampler)
                {
                    if (diagnostics)
                        *diagnostics += "Reflected sampler missing in base_def: " + resource.name + "\n";
                    return false;
                }

                AddFixedTextureSampler(out_requirements.samplers,
                                       slot,
                                       base_sampler->sampler_type,
                                       base_sampler->set_type,
                                       base_sampler->atlas_cols,
                                       base_sampler->atlas_rows,
                                       base_sampler->channel_hint);
            }
        }

        return true;
    }
}

bool CollectShaderAutoRequirements(const FixedMaterialDef &base_def,
                                   const std::string &shader_library_path,
                                   const std::string &vertex_glsl,
                                   const std::string &fragment_glsl,
                                   ShaderAutoRequirements &out_requirements,
                                   std::string *diagnostics)
{
    out_requirements.ubos.clear();
    out_requirements.ssbos.clear();
    out_requirements.samplers.clear();

    if (diagnostics)
        diagnostics->clear();

    FixedUBODescriptors reflection_seed_ubos;
    FixedSSBODescriptors reflection_seed_ssbos;
    FixedTextureSamplerDescriptors reflection_seed_samplers;

    BuildReflectionSeedDescriptors(base_def.ubo_descriptors, reflection_seed_ubos);
    BuildReflectionSeedDescriptors(base_def.ssbo_descriptors, reflection_seed_ssbos);
    BuildReflectionSeedDescriptors(base_def.texture_samplers, reflection_seed_samplers);

    FixedMaterialDef reflection_seed_def = base_def;
    reflection_seed_def.ubo_descriptors = reflection_seed_ubos.empty() ? nullptr : &reflection_seed_ubos;
    reflection_seed_def.ssbo_descriptors = reflection_seed_ssbos.empty() ? nullptr : &reflection_seed_ssbos;
    reflection_seed_def.texture_samplers = reflection_seed_samplers.empty() ? nullptr : &reflection_seed_samplers;

    std::string prepared_vs;
    std::string prepared_fs;
    if (!PrepareCompositorGLSLForReflection(reflection_seed_def,
                                            vertex_glsl,
                                            fragment_glsl,
                                            prepared_vs,
                                            prepared_fs,
                                            diagnostics))
    {
        return false;
    }

    std::vector<ReflectedResource> resources;
    if (!CollectReflectedStageResources(uint32_t(VK_SHADER_STAGE_VERTEX_BIT), prepared_vs, resources, diagnostics))
        return false;

    if (!CollectReflectedStageResources(uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), prepared_fs, resources, diagnostics))
        return false;

    if (!BuildReflectedRequirements(base_def, resources, out_requirements, diagnostics))
        return false;

    std::fprintf(stderr,
                 "[ShaderRequireScanner] reflection collection: shader_lib='%s', vs_bytes=%zu, fs_bytes=%zu\n",
                 shader_library_path.c_str(),
                 prepared_vs.size(),
                 prepared_fs.size());
    DumpCollectedRequirements(out_requirements);

    return true;
}

void MergeShaderAutoRequirements(const FixedMaterialDef &base_def,
                                 const ShaderAutoRequirements &auto_requirements,
                                 FixedMaterialDef &out_def,
                                 FixedUBODescriptors &ubo_storage,
                                 FixedSSBODescriptors &ssbo_storage,
                                 FixedTextureSamplerDescriptors &sampler_storage)
{
    ubo_storage.clear();
    ssbo_storage.clear();
    sampler_storage.clear();

    if (base_def.ubo_descriptors)
        ubo_storage = *base_def.ubo_descriptors;

    if (base_def.ssbo_descriptors)
        ssbo_storage = *base_def.ssbo_descriptors;

    if (base_def.texture_samplers)
        sampler_storage = *base_def.texture_samplers;

    for (const auto semantic : auto_requirements.ubos)
        AddFixedUBODescriptor(ubo_storage, semantic);

    for (const auto semantic : auto_requirements.ssbos)
        AddFixedSSBODescriptor(ssbo_storage, semantic);

    for (const auto &[slot, sampler] : auto_requirements.samplers)
        sampler_storage.try_emplace(slot, sampler);

    out_def = base_def;
    out_def.ubo_descriptors = ubo_storage.empty() ? nullptr : &ubo_storage;
    out_def.ssbo_descriptors = ssbo_storage.empty() ? nullptr : &ssbo_storage;
    out_def.texture_samplers = sampler_storage.empty() ? nullptr : &sampler_storage;
}
}
