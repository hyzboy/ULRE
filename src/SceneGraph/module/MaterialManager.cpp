#include<hgl/graph/module/MaterialManager.h>
#include<hgl/vk/pipeline/VKGraphicsPipelineLayoutData.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKObjectNameBuilder.h>
#include<hgl/vk/VKMaterialTemplate.h>
#include<hgl/vk/VKMaterialParameters.h>
#include<hgl/vk/VKMaterialResourceDomain.h>
#include<hgl/vk/VKDomainMaterialBinding.h>
#include<hgl/vk/VKShaderModule.h>
#include<hgl/vk/VKShaderModuleMap.h>
#include<hgl/vk/VKMaterialDescriptorManager.h>
#include<hgl/vk/VKVertexInput.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/MaterialCreatePrecheckAdapter.h>
#include<hgl/graph/module/MaterialFinalizeFlowAdapter.h>
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/ShaderCreateInfoVertex.h>
#include<hgl/mtl/Material2DCreateConfig.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/object/ObjectTracker.h>
#include<cstdio>
#include<cstring>
#include<cstdint>
#include<vector>
#include<algorithm>

namespace hgl::graph{

bool MaterialSpec::IsValid() const
{
    switch (family)
    {
        case Family::Preset2D:
            return cfg2d != nullptr;
        case Family::Preset3D:
            return cfg3d != nullptr;
        case Family::Variant2D:
            return variant_key != nullptr && cfg2d != nullptr;
        case Family::Variant3D:
            return variant_key != nullptr && cfg3d != nullptr;
        default:
            return false;
    }
}

namespace
{
    void CreateShaderStageList(ShaderStageCreateInfoList &shader_stage_list,ShaderModuleMap *shader_maps)
    {
        const ShaderModule *sm;

        const int shader_count=shader_maps->GetCount();
        shader_stage_list.resize(static_cast<size_t>(shader_count));

        VkPipelineShaderStageCreateInfo *p=shader_stage_list.data();

        for(auto [stage, module] : *shader_maps)
        {
            sm = module;
            mem_copy(p,sm->GetCreateInfo(),1);

            ++p;
        }
    }

    bool BuildLegacyShaderModules(MaterialManager *manager,
                                  const std::string &mtl_name,
                                  const ShaderStageMap &sci_map,
                                  ShaderModuleMap *shader_maps)
    {
        if (!manager || !shader_maps)
        {
            std::fprintf(stderr,
                "[MaterialManager] BuildLegacyShaderModules failed for '%s': manager=%p shader_maps=%p\n",
                mtl_name.c_str(),
                manager,
                shader_maps);
            return false;
        }

        if (sci_map.GetCount() < 2)
        {
            std::fprintf(stderr,
                "[MaterialManager] BuildLegacyShaderModules failed for '%s': shader count=%d (expected >= 2)\n",
                mtl_name.c_str(),
                sci_map.GetCount());
            return false;
        }

        for (auto [stage, sci_ptr] : sci_map)
        {
            (void)stage;

            if (!sci_ptr)
            {
                std::fprintf(stderr,
                    "[MaterialManager] BuildLegacyShaderModules failed for '%s': shader create info is null\n",
                    mtl_name.c_str());
                return false;
            }

            const ShaderModule *module = manager->CreateShaderModule(mtl_name, sci_ptr);
            if (!module)
            {
                std::fprintf(stderr,
                    "[MaterialManager] BuildLegacyShaderModules failed for '%s': CreateShaderModule returned null for stage=%u\n",
                    mtl_name.c_str(),
                    static_cast<unsigned>(sci_ptr->GetShaderStage()));
                return false;
            }

            shader_maps->Add(module);
        }

        return true;
    }

}//namespace

GRAPH_MODULE_CONSTRUCT(MaterialManager)
{
    // Phase E: entry[0] is the invalid sentinel (domain_id == 0 means "no domain")
    domain_table_.push_back({nullptr, 0u});
}

const ShaderModule *MaterialManager::CreateShaderModule(const std::string &sm_name,const ShaderCreateInfo *sci)
{
    VulkanDevice *device = GetDevice();
    if(!device)
    {
        std::fprintf(stderr, "[MaterialManager] CreateShaderModule failed: device is null for '%s'\n", sm_name.c_str());
        return(nullptr);
    }
    if(sm_name.empty())
    {
        std::fprintf(stderr, "[MaterialManager] CreateShaderModule failed: shader module name is empty\n");
        return(nullptr);
    }
    if(!sci)
    {
        std::fprintf(stderr, "[MaterialManager] CreateShaderModule failed for '%s': ShaderCreateInfo is null\n", sm_name.c_str());
        return(nullptr);
    }

    const int bit_offset=GetBitOffset((uint32_t)sci->GetShaderStage());

    if(bit_offset<0||bit_offset>VK_SHADER_STAGE_TYPE_COUNT)
    {
        std::fprintf(stderr,
            "[MaterialManager] CreateShaderModule failed for '%s': invalid stage bit offset=%d stage=%u\n",
            sm_name.c_str(),
            bit_offset,
            static_cast<unsigned>(sci->GetShaderStage()));
        return(nullptr);
    }

    ShaderModule *sm;

    ShaderModuleMapByName &sm_map=shader_module_by_name[bit_offset];

    if(auto it = sm_map.find(sm_name); it != sm_map.end())
        return it->second;

    sm=device->CreateShaderModule((VkShaderStageFlagBits)sci->GetShaderStage(),sci->GetSPVData(),sci->GetSPVSize());

    if(!sm)
    {
        std::fprintf(stderr,
            "[MaterialManager] CreateShaderModule failed for '%s': VulkanDevice::CreateShaderModule returned null (stage=%u spv_size=%zu)\n",
            sm_name.c_str(),
            static_cast<unsigned>(sci->GetShaderStage()),
            sci->GetSPVSize());
        return(nullptr);
    }

    sm_map.emplace(sm_name,sm);

    #ifdef _DEBUG
        {
            DebugUtils *du=device->GetDebugUtils();

            if(du)
            {
                std::string shader_name = "Shader:" + sm_name + ":" + GetShaderStageName((VkShaderStageFlagBits)sci->GetShaderStage());
                du->SetShaderModule(*sm, shader_name.c_str());
            }
        }
    #endif//_DEBUG

    return sm;
}

const ShaderModule *MaterialManager::CreateShaderModuleFromSPV(const std::string &sm_name,
                                                                const VkShaderStageFlagBits stage,
                                                                const uint32_t *spv_data,
                                                                const size_t spv_size)
{
    VulkanDevice *device = GetDevice();
    if(!device)return(nullptr);
    if(sm_name.empty())return(nullptr);
    if(!spv_data||spv_size==0)return(nullptr);

    const int bit_offset=GetBitOffset((uint32_t)stage);

    if(bit_offset<0||bit_offset>VK_SHADER_STAGE_TYPE_COUNT)return(nullptr);

    ShaderModule *sm;

    ShaderModuleMapByName &sm_map=shader_module_by_name[bit_offset];

    if(auto it = sm_map.find(sm_name); it != sm_map.end())
        return it->second;

    sm=device->CreateShaderModule(stage,spv_data,spv_size);

    if(!sm)
        return(nullptr);

    sm_map.emplace(sm_name,sm);

    #ifdef _DEBUG
        {
            DebugUtils *du=device->GetDebugUtils();

            if(du)
            {
                std::string shader_name = "Shader:" + sm_name + ":" + GetShaderStageName(stage);
                du->SetShaderModule(*sm, shader_name.c_str());
            }
        }
    #endif//_DEBUG

    return sm;
}

GraphicsPipelineLayoutData *MaterialManager::CreateMaterialGraphicsPipelineLayoutData(const std::string &mtl_name, const MaterialDescriptorManager *desc_manager)
{
    VulkanDevice *device = GetDevice();
    if(!device)
    {
        std::fprintf(stderr,
            "[MaterialManager] CreateMaterialGraphicsPipelineLayoutData failed for '%s': device is null\n",
            mtl_name.c_str());
        return nullptr;
    }

    GraphicsPipelineLayoutData *pld = device->CreateGraphicsPipelineLayoutData(desc_manager);

    if(pld)
    {
        #ifdef _DEBUG
            DebugUtils *du = device->GetDebugUtils();
            if(du)
            {
                const std::string name = "PipelineLayout:" + mtl_name;
                du->SetPipelineLayout(pld->pipeline_layout, name.c_str());
            }
        #endif//_DEBUG
    }

    return pld;
}

MaterialParameters *MaterialManager::CreateMaterialMP(const std::string &mtl_name, const MaterialDescriptorManager *desc_manager, const GraphicsPipelineLayoutData *pld, const DescriptorSetType &desc_set_type)
{
    VulkanDevice *device = GetDevice();
    if(!device)
    {
        std::fprintf(stderr,
            "[MaterialManager] CreateMaterialMP failed for '%s': device is null (set=%d)\n",
            mtl_name.c_str(),
            static_cast<int>(desc_set_type));
        return nullptr;
    }

    MaterialParameters *mp = device->CreateMP(desc_manager, pld, desc_set_type);

    if(mp)
    {
        #ifdef _DEBUG
            DebugUtils *du = device->GetDebugUtils();
            if(du)
            {
                std::string debug_name = mtl_name + ":" + GetDescriptorSetTypeName(desc_set_type);
                const std::string ds_name = "DescSet:" + debug_name;
                const std::string dsl_name = "DescSetLayout:" + debug_name;
                du->SetDescriptorSet(mp->GetVkDescriptorSet(), ds_name.c_str());
                du->SetDescriptorSetLayout(pld->layouts[static_cast<int>(desc_set_type)], dsl_name.c_str());
            }
        #endif//_DEBUG
    }

    return mp;
}

void MaterialManager::ApplyMaterialFinalizePlan(MaterialTemplate *mtl, const std::string &mtl_name, const mtl::MaterialCreateInfo &mci)
{
    if(!mtl)
        return;

    MaterialFinalizePlan finalize_plan;
    BuildMaterialFinalizePlan(mtl->desc_manager, mci, finalize_plan);

    mtl->pipeline_layout_data = CreateMaterialGraphicsPipelineLayoutData(mtl_name, mtl->desc_manager);

    for(const auto set_type : finalize_plan.mp_set_types)
    {
        mtl->mp_array[(int)set_type] = CreateMaterialMP(mtl_name, mtl->desc_manager, mtl->pipeline_layout_data, set_type);
    }

    mtl->required_instance_layout = finalize_plan.required_instance_layout;
    mtl->mi_max_count  = finalize_plan.mi_max_count;
}

MaterialTemplate *MaterialManager::TryGetCachedMaterial(const std::string &name)
{
    acquire_material_cache_lookups.fetch_add(1);

    if(auto it = material_by_name.find(name); it != material_by_name.end())
    {
        acquire_material_cache_hits.fetch_add(1);
        return it->second;
    }

    acquire_material_cache_misses.fetch_add(1);

    return nullptr;
}

MaterialTemplate *MaterialManager::TryInitializeFallbackMaterial()
{
    if(fallback_material)
        return fallback_material;

    // Try to create a Checkerboard3D material as fallback
    // If Checkerboard3D is not available, fallback to Standard

    static const std::string fallback_name("__sys__fallback_checkerboard3d");

    // Check if already cached from a previous attempt
    fallback_material = TryGetCachedMaterial(fallback_name);
    if(fallback_material)
        return fallback_material;

    // Try to create Checkerboard3D preset
    mtl::Material3DCreateConfig cfg;

    fallback_material = CreateMaterial(mtl::MaterialPreset::Checkerboard3D, &cfg);

    if(!fallback_material)
    {
        // If Checkerboard3D fails, try Standard as ultimate fallback
        std::fprintf(stderr,
            "[MaterialManager] TryInitializeFallbackMaterial failed creating Checkerboard3D, trying Standard\n");

        fallback_material = CreateMaterial(mtl::MaterialPreset::Standard, &cfg);

        if(!fallback_material)
        {
            std::fprintf(stderr,
                "[MaterialManager] TryInitializeFallbackMaterial failed: could not create any fallback material\n");
            return nullptr;
        }
    }

    std::fprintf(stdout,
        "[MaterialManager] Fallback material initialized: %s\n",
        fallback_material->GetName().c_str());

    return fallback_material;
}

MaterialTemplate *MaterialManager::GetFallbackMaterial()
{
    if(!fallback_material)
        TryInitializeFallbackMaterial();

    return fallback_material;
}

MaterialResourceDomain *MaterialManager::GetOrCreateDefaultDomain(MaterialTemplate *mtl)
{
    if (!mtl || !mtl->hasMI()) return nullptr;
    auto it = default_domain_map.find(mtl);
    if (it != default_domain_map.end()) return it->second;
    MaterialResourceDomain *domain = CreateMaterialResourceDomain(mtl);
    default_domain_map[mtl] = domain;
    return domain;
}

MaterialTemplate *MaterialManager::ResolveMaterial(const MaterialInstance *mi) const
{
    return mi ? mi->GetMaterial() : nullptr;
}

bool MaterialManager::ExecuteMaterialBuildPipeline(MaterialTemplate *mtl,
                                                   const std::string &mtl_name,
                                                   const mtl::MaterialCreateInfo *mci,
                                                   const ShaderStageMap &sci_map)
{
    if(!mtl || !mci)
    {
        std::fprintf(stderr,
            "[MaterialManager] ExecuteMaterialBuildPipeline failed for '%s': mtl=%p mci=%p\n",
            mtl_name.c_str(),
            mtl,
            mci);
        return false;
    }

    if(!BuildLegacyShaderModules(this,
                                 mtl_name,
                                 sci_map,
                                 mtl->shader_maps))
    {
        std::fprintf(stderr,
            "[MaterialManager] ExecuteMaterialBuildPipeline failed for '%s': BuildLegacyShaderModules returned false\n",
            mtl_name.c_str());
        return false;
    }

    CreateShaderStageList(mtl->shader_stage_list,mtl->shader_maps);

    const ShaderCreateInfoVertex *vert = mci->GetVertexShader();
    mtl->vertex_input = vert ? GetVertexInput(vert->GetInput()) : nullptr;

    const auto &mdi = mci->GetDescriptorInfo();
    if(mdi.GetCount() > 0)
        mtl->desc_manager = new MaterialDescriptorManager(mtl_name, mdi.Get());
    else
        mtl->desc_manager = nullptr;

    ApplyMaterialFinalizePlan(mtl, mtl_name, *mci);

    return true;
}

MaterialTemplate *MaterialManager::CreateMaterial(const std::string &mtl_name,const mtl::MaterialCreateInfo *mci)
{
    HGL_CAPTURE_SCOPE();

    if(!mci)
    {
        std::fprintf(stderr,
            "[MaterialManager] CreateMaterial(name,mci) failed for '%s': mci is null\n",
            mtl_name.c_str());
        return(nullptr);
    }

    MaterialCreatePrecheckResult precheck_result;
    const AnsiString mtl_name_ansi(mtl_name.c_str());
    const MaterialCreatePrecheckDecision precheck_decision = RunMaterialCreatePrecheck(
        mci,
        mtl_name_ansi,
        [&](const AnsiString &name)->MaterialTemplate * { return TryGetCachedMaterial(name.c_str()); },
        precheck_result);

    if(precheck_decision == MaterialCreatePrecheckDecision::UseCached)
        return precheck_result.cached_material;

    if(precheck_decision != MaterialCreatePrecheckDecision::Proceed)
    {
        std::fprintf(stderr,
            "[MaterialManager] CreateMaterial(name,mci) failed for '%s': precheck decision=%d\n",
            mtl_name.c_str(),
            static_cast<int>(precheck_decision));
        return nullptr;
    }

    const ShaderStageMap &sci_map = *precheck_result.shader_map;

    AutoDelete<MaterialTemplate> mtl=new MaterialTemplate(mtl_name,mci);
    if(!ExecuteMaterialBuildPipeline(mtl,
                                     mtl_name,
                                     mci,
                                     sci_map))
    {
        std::fprintf(stderr,
            "[MaterialManager] CreateMaterial(name,mci) failed for '%s': ExecuteMaterialBuildPipeline returned false\n",
            mtl_name.c_str());
        return nullptr;
    }

    Add(mtl);
    acquire_material_created.fetch_add(1);

    material_by_name.emplace(mtl_name,mtl);
    // MaterialTemplate is a C++ object managed by MaterialManager, not a Vulkan object
    // No need to track with ObjectTracker
    return mtl.Finish();
}

MaterialTemplate *MaterialManager::CreateMaterial(const mtl::MaterialPreset mtl_id,mtl::Material2DCreateConfig *cfg)
{
    HGL_CAPTURE_SCOPE();

    if(!cfg)
        return(nullptr);

    mtl::MaterialVariantKey key = mtl::MapPresetToVariantKey(mtl_id);
    mtl::ApplyCreateConfigToVariantKey(key, cfg);
    return CreateMaterial(key, cfg);
}

MaterialTemplate *MaterialManager::AcquireMaterial(const MaterialSpec &spec, MaterialSpecKey *out_key)
{
    if(!spec.IsValid())
        return nullptr;

    MaterialTemplate *result = nullptr;

    switch(spec.family)
    {
        case MaterialSpec::Family::Preset2D:
            result = AcquireMaterial(spec.preset, spec.cfg2d, out_key);
            break;
        case MaterialSpec::Family::Preset3D:
            result = AcquireMaterial(spec.preset, spec.cfg3d, out_key);
            break;
        case MaterialSpec::Family::Variant2D:
            result = AcquireMaterial(*spec.variant_key, spec.cfg2d, out_key);
            break;
        case MaterialSpec::Family::Variant3D:
            result = AcquireMaterial(*spec.variant_key, spec.cfg3d, out_key);
            break;
        default:
            result = nullptr;
            break;
    }

    return result;
}

MaterialTemplate *MaterialManager::AcquireMaterial(const mtl::MaterialPreset mtl_id, mtl::Material2DCreateConfig *cfg, MaterialSpecKey *out_key)
{
    acquire_material_requests.fetch_add(1);

    MaterialTemplate *mtl = CreateMaterial(mtl_id, cfg);

    if(!mtl)
    {
        mtl = GetFallbackMaterial();
        if(mtl)
            acquire_fallback_used.fetch_add(1);
    }

    if(out_key)
        out_key->cache_name = mtl ? mtl->GetName() : std::string();

    return mtl;
}

MaterialTemplate *MaterialManager::AcquireMaterial(const mtl::MaterialPreset mtl_id, mtl::Material3DCreateConfig *cfg, MaterialSpecKey *out_key)
{
    acquire_material_requests.fetch_add(1);

    MaterialTemplate *mtl = CreateMaterial(mtl_id, cfg);

    if(!mtl)
    {
        mtl = GetFallbackMaterial();
        if(mtl)
            acquire_fallback_used.fetch_add(1);
    }

    if(out_key)
        out_key->cache_name = mtl ? mtl->GetName() : std::string();

    return mtl;
}

MaterialTemplate *MaterialManager::AcquireMaterial(const mtl::MaterialVariantKey &key, mtl::Material2DCreateConfig *cfg, MaterialSpecKey *out_key)
{
    acquire_material_requests.fetch_add(1);

    MaterialTemplate *mtl = CreateMaterial(key, cfg);

    if(!mtl)
    {
        mtl = GetFallbackMaterial();
        if(mtl)
            acquire_fallback_used.fetch_add(1);
    }

    if(out_key)
        out_key->cache_name = mtl ? mtl->GetName() : std::string();

    return mtl;
}

MaterialTemplate *MaterialManager::AcquireMaterial(const mtl::MaterialVariantKey &key, mtl::Material3DCreateConfig *cfg, MaterialSpecKey *out_key)
{
    acquire_material_requests.fetch_add(1);

    MaterialTemplate *mtl = CreateMaterial(key, cfg);

    if(!mtl)
    {
        mtl = GetFallbackMaterial();
        if(mtl)
            acquire_fallback_used.fetch_add(1);
    }

    if(out_key)
        out_key->cache_name = mtl ? mtl->GetName() : std::string();

    return mtl;
}

void MaterialManager::ResetShaderGenProfiler()
{
    // Legacy-only mode keeps ShaderGen debug APIs as no-op for compatibility.
}

ShaderGenProfilerSnapshot MaterialManager::GetShaderGenProfilerSnapshot() const
{
    return {};
}

bool MaterialManager::GetShaderGenLastValidationReport(ShaderGenValidationReport &out_report, std::string *out_material_name) const
{
    out_report = {};
    if (out_material_name)
        out_material_name->clear();
    return false;
}

std::vector<ShaderGenValidationReportRecord> MaterialManager::GetShaderGenRecentValidationReports(const uint32_t max_count) const
{
    (void)max_count;
    return {};
}

std::map<std::string, uint32_t> MaterialManager::GetShaderGenRecentValidationCategoryHistogram(const uint32_t max_count) const
{
    (void)max_count;
    return {};
}

MaterialTemplate *MaterialManager::CreateMaterial(const mtl::MaterialPreset mtl_id,mtl::Material3DCreateConfig *cfg)
{
    HGL_CAPTURE_SCOPE();

    if(!cfg)
    {
        std::fprintf(stderr,
            "[MaterialManager] CreateMaterial(preset=Standard/3D) failed: cfg is null (preset=%s)\n",
            mtl::GetMaterialPresetName(mtl_id));
        return(nullptr);
    }

    mtl::MaterialVariantKey key = mtl::MapPresetToVariantKey(mtl_id);
    mtl::ApplyCreateConfigToVariantKey(key, cfg);
    return CreateMaterial(key, cfg);
}

MaterialTemplate *MaterialManager::CreateMaterial(const mtl::MaterialVariantKey &key,mtl::Material2DCreateConfig *cfg)
{
    HGL_CAPTURE_SCOPE();

    if(!cfg)
        return(nullptr);

    std::string hash_name="variant";
    char key_hash[32] = {};
    std::snprintf(key_hash, sizeof(key_hash), "%llu", static_cast<unsigned long long>(key.Hash()));
    std::printf("[DEBUG] CreateMaterial hash=%s ST=%d GM=%d tex=%u sampler=%u\n", key_hash, int(key.surface_type), int(key.geometry_mode), key.texture_source_bits, key.sampler_feature_bits);
    hash_name+="#";
    hash_name+=key_hash;
    hash_name+="?";
    hash_name+=cfg->ToHashStdString().c_str();

    {
        MaterialTemplate *cached = TryGetCachedMaterial(hash_name);
        if (cached)
            return cached;
    }

    const auto *profile=GetPhysicalDeviceProfile();

    AutoDelete<mtl::MaterialCreateInfo> mci=mtl::CreateMaterialCreateInfo(profile,key,cfg);

    if(!mci)
        return(nullptr);

    MaterialTemplate *mat = this->CreateMaterial(hash_name,mci);
    if (mat)
    {
        uint8_t flags = 0;
        for (uint8_t s = 0; s < uint8_t(mtl::SamplerSlot::RANGE_SIZE); ++s)
            if (key.GetTextureSourceMode(mtl::SamplerSlot(s)) == mtl::TextureSourceMode::Array)
                flags |= (1u << s);
        mat->SetTextureArraySlotFlags(flags);
    }
    return mat;
}

MaterialTemplate *MaterialManager::CreateMaterial(const mtl::MaterialVariantKey &key,mtl::Material3DCreateConfig *cfg)
{
    HGL_CAPTURE_SCOPE();

    if(!cfg)
    {
        std::fprintf(stderr, "[MaterialManager] CreateMaterial(key/3D) failed: cfg is null\n");
        return(nullptr);
    }

    std::string hash_name="variant";
    char key_hash[32] = {};
    std::snprintf(key_hash, sizeof(key_hash), "%llu", static_cast<unsigned long long>(key.Hash()));
    hash_name+="#";
    hash_name+=key_hash;
    hash_name+="?";
    hash_name+=cfg->ToHashStdString().c_str();

    {
        MaterialTemplate *cached = TryGetCachedMaterial(hash_name);
        if (cached)
            return cached;
    }

    const auto *profile=GetPhysicalDeviceProfile();
    if(!profile)
    {
        std::fprintf(stderr,
            "[MaterialManager] CreateMaterial(key/3D) warning: physical device profile is null (key_hash=%llu)\n",
            static_cast<unsigned long long>(key.Hash()));
    }

    AutoDelete<mtl::MaterialCreateInfo> mci=mtl::CreateMaterialCreateInfo(profile,key,cfg);

    if(!mci)
    {
        std::fprintf(stderr,
            "[MaterialManager] CreateMaterial(key/3D) failed: CreateMaterialCreateInfo returned null (key_hash=%llu surface=%u geom=%u tex_bits=0x%08X sampler_bits=0x%08X va_bits=0x%08X extra_bits=0x%08X cfg_hash=%s)\n",
            static_cast<unsigned long long>(key.Hash()),
            static_cast<unsigned>(key.surface_type),
            static_cast<unsigned>(key.geometry_mode),
            key.texture_source_bits,
            key.sampler_feature_bits,
            key.vertex_attribute_feature_bits,
            key.extra_feature_bits,
            cfg->ToHashStdString().c_str());
        return(nullptr);
    }

    MaterialTemplate *mat = this->CreateMaterial(hash_name,mci);
    if (mat)
    {
        uint8_t flags = 0;
        for (uint8_t s = 0; s < uint8_t(mtl::SamplerSlot::RANGE_SIZE); ++s)
            if (key.GetTextureSourceMode(mtl::SamplerSlot(s)) == mtl::TextureSourceMode::Array)
                flags |= (1u << s);
        mat->SetTextureArraySlotFlags(flags);
    }
    return mat;
}

MaterialInstance *MaterialManager::AcquireMaterialInstance(const MaterialInstanceSpec &spec, MaterialInstanceSpecKey *out_key)
{
    static bool s_warned_legacy_acquire_mi = false;
    if (!s_warned_legacy_acquire_mi)
    {
        s_warned_legacy_acquire_mi = true;
        std::fprintf(stderr,
            "[MaterialManager] AcquireMaterialInstance is deprecated. "
            "Prefer slot-first APIs: AllocMaterialInstanceSlot or MaterialAssetRegistry::ResolveMI(...).\n");
    }

    acquire_mi_requests.fetch_add(1);

    if(!spec.IsValid())
        return nullptr;

    MaterialInstance *mi = nullptr;

    if(spec.domain)
    {
        if(!spec.material) return nullptr;
        const VIL *resolved_vil = spec.vil_cfg ? spec.material->CreateVIL(spec.vil_cfg)
                                               : (spec.vil ? spec.vil : spec.material->GetDefaultVIL());
        mi = CreateMaterialInstance(spec.domain, spec.material, resolved_vil,
                                    spec.instance_data, spec.instance_data_size);
    }
    else
    {
        MaterialTemplate *mtl = spec.material;
        if(!mtl) return nullptr;

        MaterialResourceDomain *def_domain = GetOrCreateDefaultDomain(mtl);
        if(!def_domain) return nullptr;

        const VIL *use_vil = spec.vil_cfg ? mtl->CreateVIL(spec.vil_cfg)
                                          : (spec.vil ? spec.vil : mtl->GetDefaultVIL());

        mi = CreateMaterialInstance(def_domain, mtl, use_vil,
                                    spec.instance_data, spec.instance_data_size);
        if(mi)
        {
            VulkanDevice *device = GetDevice();
            if(device)
                device->TrackObject(VK_OBJECT_TYPE_UNKNOWN, (uint64_t)(uintptr_t)mi,
                                    ObjectNameBuilder(mtl->GetName()).Append(ObjectTypeTag::MaterialInstance));
        }
    }

    if(!mi)
        return nullptr;

    acquire_mi_created.fetch_add(1);

    mi->SetRenderPreset(spec.preset);

    if(out_key)
    {
        const PrimitiveMaterialSlot s = mi->ToSlot();
        out_key->material = s.material_template;
        out_key->vil      = s.vil;
        out_key->preset   = s.preset;
        out_key->domain   = s.domain;
    }

    return mi;
}

bool MaterialManager::UpdateInstanceData(MaterialInstance *mi, const void *data, const uint32 data_size)
{
    if(!mi || !data || data_size == 0)
        return false;

    mi->WriteMIData(data, data_size);
    return true;
}

// ============================================================================
// Phase E: domain_id + generation 句柄表
// ============================================================================

uint32_t MaterialManager::RegisterDomain(MaterialResourceDomain *domain)
{
    if (!domain)
        return 0;

    auto it = domain_id_map_.find(domain);
    if (it != domain_id_map_.end())
        return it->second;

    const uint32_t id = static_cast<uint32_t>(domain_table_.size());
    domain_table_.push_back({domain, 1u});
    domain_id_map_[domain] = id;
    return id;
}

void MaterialManager::UnregisterDomain(MaterialResourceDomain *domain)
{
    if (!domain)
        return;

    auto it = domain_id_map_.find(domain);
    if (it == domain_id_map_.end())
        return;

    const uint32_t id = it->second;
    domain_id_map_.erase(it);

    if (id < static_cast<uint32_t>(domain_table_.size()))
        domain_table_[id].domain = nullptr;
    // generation is intentionally kept so stale MI handles resolve to nullptr
}

MaterialResourceDomain *MaterialManager::ResolveDomain(uint32_t domain_id, uint32_t generation) const
{
    if (domain_id == 0 || domain_id >= static_cast<uint32_t>(domain_table_.size()))
        return nullptr;

    const DomainEntry &e = domain_table_[domain_id];
    if (e.generation != generation || !e.domain)
        return nullptr;

    return e.domain;
}

void MaterialManager::ReplaceDomain(uint32_t domain_id, MaterialResourceDomain *new_domain)
{
    if (domain_id == 0 || domain_id >= static_cast<uint32_t>(domain_table_.size()))
        return;

    DomainEntry &e = domain_table_[domain_id];

    // Evict old entry from reverse map
    if (e.domain)
        domain_id_map_.erase(e.domain);

    // Install new domain and increment generation (invalidates all old MI handles)
    e.domain = new_domain;
    ++e.generation;

    if (new_domain)
        domain_id_map_[new_domain] = domain_id;
}

// ============================================================================
// Phase A: Slot-first API — AllocMaterialInstanceSlot
// ============================================================================

PrimitiveMaterialSlot MaterialManager::AllocMaterialInstanceSlot(
    MaterialResourceDomain *domain,
    MaterialTemplate *material,
    const VIL *vil,
    GraphicsPipelinePreset preset,
    const void *instance_data,
    uint32_t instance_data_size)
{
    alloc_slot_requests.fetch_add(1);

    if (!domain || !material)
    {
        alloc_slot_failed.fetch_add(1);
        return {}; // return empty slot
    }

    const VIL *use_vil = vil ? vil : material->GetDefaultVIL();
    if (!use_vil)
    {
        alloc_slot_failed.fetch_add(1);
        return {};
    }

    const bool needs_mi = material->hasMI();
    int mi_id = -1;

    if (needs_mi)
        alloc_slot_with_mi.fetch_add(1);
    else
        alloc_slot_no_mi.fetch_add(1);

    if (needs_mi)
    {
        mi_id = domain->AllocMISlot();
        if (mi_id < 0)
        {
            std::fprintf(stderr,
                "[MaterialManager] AllocMaterialInstanceSlot failed: material='%s' requires MI but domain allocation failed\n",
                material->GetName().c_str());
            alloc_slot_failed.fetch_add(1);
            return {}; // allocation failed
        }
    }

    // Write instance data if provided
    if (instance_data && instance_data_size > 0)
    {
        if (!needs_mi)
        {
            std::fprintf(stderr,
                "[MaterialManager] AllocMaterialInstanceSlot rejected payload: material='%s' has no MI layout (size=%u)\n",
                material->GetName().c_str(),
                static_cast<unsigned>(instance_data_size));
            alloc_slot_no_mi_payload_rejected.fetch_add(1);
            alloc_slot_failed.fetch_add(1);
            return {}; // non-MI material should not receive MI payload
        }

        void *mi_data = domain->GetMIData(mi_id);
        if (mi_data)
            std::memcpy(mi_data, instance_data, instance_data_size);
    }

    // Build and return slot
    PrimitiveMaterialSlot slot;
    slot.material_template = material;
    slot.domain = domain;
    slot.mi_id = mi_id;
    slot.vil = use_vil;
    slot.preset = preset;
    alloc_slot_created.fetch_add(1);
    return slot;
}

// ============================================================================
// MaterialResourceDomain — Phase 1
// ============================================================================

MaterialResourceDomain *MaterialManager::CreateMaterialResourceDomain(
    mtl::InstanceDataLayout layout, uint32_t max_count, uint8_t tex_array_slots)
{
    return new MaterialResourceDomain(layout, max_count, tex_array_slots);
}

// Convenience overload: create a domain that matches the given MaterialTemplate's requirements.
// Phase E will remove this once all callsites pass layout + texslots explicitly.
MaterialResourceDomain *MaterialManager::CreateMaterialResourceDomain(MaterialTemplate *mtl)
{
    if(!mtl)
        return nullptr;

    return CreateMaterialResourceDomain(
        mtl->GetRequiredLayout(),
        mtl->GetMIMaxCount(),
        mtl->GetTextureArraySlotFlags());
}

DomainMaterialBinding *MaterialManager::CreateDomainMaterialBinding(MaterialResourceDomain *domain, MaterialTemplate *mtl)
{
    if (!domain || !mtl)
        return nullptr;

    // Phase D: semantic layout match — enum comparison prevents stride collisions
    // (e.g. Color4f=16B vs PBRStandard=16B share the same stride but are incompatible)
    if (domain->GetLayout() != mtl->GetRequiredLayout())
    {
        std::fprintf(stderr,
            "[MaterialManager] CreateDomainMaterialBinding: layout mismatch "
            "domain=%s mtl=%s\n",
            mtl::GetInstanceDataName(domain->GetLayout()),
            mtl::GetInstanceDataName(mtl->GetRequiredLayout()));
        return nullptr;
    }

    // Phase D: bitmask subset check — Domain must supply every TextureArray slot the Template requires
    const uint8_t required_slots = mtl->GetTextureArraySlotFlags();
    if (required_slots != 0 &&
        (domain->GetTextureArraySlots() & required_slots) != required_slots)
    {
        std::fprintf(stderr,
            "[MaterialManager] CreateDomainMaterialBinding: texture array slot mismatch "
            "domain=0x%02x required=0x%02x\n",
            unsigned(domain->GetTextureArraySlots()),
            unsigned(required_slots));
        return nullptr;
    }

    VulkanDevice *device = GetDevice();
    if (!device)
        return nullptr;

    MaterialParameters *mp_per_material = nullptr;
    if (mtl->hasSet(DescriptorSetType::PerMaterial))
        mp_per_material = CreateMaterialMP(mtl->GetName(), mtl->desc_manager,
                                           mtl->pipeline_layout_data, DescriptorSetType::PerMaterial);

    DomainMaterialBinding *binding = new DomainMaterialBinding(domain, mtl, mp_per_material);

    // Phase 3: register binding for lifecycle tracking
    domain_bindings_map[domain].push_back(binding);

    return binding;
}

// Phase 3 -- domain lifecycle management

void MaterialManager::ReleaseDomainMaterialBinding(DomainMaterialBinding *binding)
{
    if (!binding)
        return;

    MaterialResourceDomain *d = binding->GetDomain();
    auto it = domain_bindings_map.find(d);
    if (it != domain_bindings_map.end())
    {
        auto &vec = it->second;
        vec.erase(std::remove(vec.begin(), vec.end(), binding), vec.end());
        if (vec.empty())
            domain_bindings_map.erase(it);
    }

    delete binding;
}

void MaterialManager::ReleaseMaterialResourceDomain(MaterialResourceDomain *domain)
{
    if (!domain)
        return;

    // Phase E: remove from domain table before deleting
    UnregisterDomain(domain);

    auto it = domain_bindings_map.find(domain);
    if (it != domain_bindings_map.end())
    {
        for (auto *b : it->second)
            delete b;
        domain_bindings_map.erase(it);
    }

    delete domain;
}

MaterialInstance *MaterialManager::CreateMaterialInstance(MaterialResourceDomain *domain,
                                                          MaterialTemplate *material,
                                                          const VIL *vil,
                                                          const void *data,
                                                          const uint32 data_size)
{
    if(!domain || !material)
        return nullptr;

    const VIL *use_vil = vil ? vil : material->GetDefaultVIL();
    int mi_id = domain->AllocMISlot();

    MaterialInstance *mi = new MaterialInstance();
    mi->material          = material;
    mi->domain_resolver   = this;
    const uint32_t d_id   = RegisterDomain(domain);
    mi->domain_id         = d_id;
    mi->domain_generation = (d_id < static_cast<uint32_t>(domain_table_.size())) ? domain_table_[d_id].generation : 0u;
    mi->owns_slot         = true;
    mi->vil      = use_vil;
    mi->mi_id    = mi_id;
    mi->InitMITLayout(domain->GetTextureArraySlots());
    Add(mi);

    if(data && data_size > 0)
        mi->WriteMIData(data, data_size);

    return mi;
}

bool MaterialManager::RebindMaterialInstance(MaterialInstance *mi, MaterialTemplate *material, const VIL *vil)
{
    if(!mi || !material)
        return false;

    mi->material = material;
    mi->vil = vil ? vil : material->GetDefaultVIL();
    // Phase E: prefer Domain slot flags as authoritative source; fall back to Template on null domain (rare)
    MaterialResourceDomain *d = mi->GetDomain();
    mi->InitMITLayout(d ? d->GetTextureArraySlots() : material->GetTextureArraySlotFlags());
    return true;
}

}//namespace hgl::graph
