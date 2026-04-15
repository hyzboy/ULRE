#include<hgl/graph/module/MaterialManager.h>
#include<hgl/vk/pipeline/VKGraphicsPipelineLayoutData.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKObjectNameBuilder.h>
#include<hgl/vk/VKMaterialTemplate.h>
#include<hgl/vk/VKMaterialParameters.h>
#include<hgl/vk/VKInstanceDataDomain.h>
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
    bool ShouldLogPow2(uint64_t v)
    {
        return v != 0 && ((v & (v - 1)) == 0);
    }

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
    // P1: IDDManager owns domain lifecycle; MaterialManager delegates to it
    idd_manager_ = new IDDManager();
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

InstanceDataDomain *MaterialManager::GetOrCreateDefaultDomain(MaterialTemplate *mtl)
{
    if (!mtl || !mtl->hasMI()) return nullptr;
    auto it = default_domain_map.find(mtl);
    if (it != default_domain_map.end())
        return idd_manager_ ? idd_manager_->Get(it->second) : nullptr;
    if (!idd_manager_) return nullptr;
    const IDDHandle handle = idd_manager_->Create(
        mtl->GetRequiredLayout(), mtl->GetMIMaxCount(), mtl->GetTextureArraySlotFlags());
    default_domain_map[mtl] = handle;
    return idd_manager_->Get(handle);
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

MaterialTemplate *MaterialManager::AcquireMaterial(const MaterialSpec &spec,
                                                   MaterialSpecKey *out_key,
                                                   MaterialAccessToken)
{
    if(!spec.IsValid())
        return nullptr;

    MaterialTemplate *result = nullptr;

    switch(spec.family)
    {
        case MaterialSpec::Family::Preset2D:
            result = AcquireMaterial(spec.preset, spec.cfg2d, out_key, MakeInternalAccessToken());
            break;
        case MaterialSpec::Family::Preset3D:
            result = AcquireMaterial(spec.preset, spec.cfg3d, out_key, MakeInternalAccessToken());
            break;
        case MaterialSpec::Family::Variant2D:
            result = AcquireMaterial(*spec.variant_key, spec.cfg2d, out_key, MakeInternalAccessToken());
            break;
        case MaterialSpec::Family::Variant3D:
            result = AcquireMaterial(*spec.variant_key, spec.cfg3d, out_key, MakeInternalAccessToken());
            break;
        default:
            result = nullptr;
            break;
    }

    return result;
}

MaterialTemplate *MaterialManager::AcquireMaterialInternal(const MaterialSpec &spec, MaterialSpecKey *out_key)
{
    return AcquireMaterial(spec, out_key, MakeInternalAccessToken());
}

MaterialTemplate *MaterialManager::AcquireMaterial(const mtl::MaterialPreset mtl_id,
                                                   mtl::Material2DCreateConfig *cfg,
                                                   MaterialSpecKey *out_key,
                                                   MaterialAccessToken)
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

MaterialTemplate *MaterialManager::AcquireMaterialInternal(const mtl::MaterialPreset mtl_id,
                                                           mtl::Material2DCreateConfig *cfg,
                                                           MaterialSpecKey *out_key)
{
    return AcquireMaterial(mtl_id, cfg, out_key, MakeInternalAccessToken());
}

MaterialTemplate *MaterialManager::AcquireMaterial(const mtl::MaterialPreset mtl_id,
                                                   mtl::Material3DCreateConfig *cfg,
                                                   MaterialSpecKey *out_key,
                                                   MaterialAccessToken)
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

MaterialTemplate *MaterialManager::AcquireMaterialInternal(const mtl::MaterialPreset mtl_id,
                                                           mtl::Material3DCreateConfig *cfg,
                                                           MaterialSpecKey *out_key)
{
    return AcquireMaterial(mtl_id, cfg, out_key, MakeInternalAccessToken());
}

MaterialTemplate *MaterialManager::AcquireMaterial(const mtl::MaterialVariantKey &key,
                                                   mtl::Material2DCreateConfig *cfg,
                                                   MaterialSpecKey *out_key,
                                                   MaterialAccessToken)
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

MaterialTemplate *MaterialManager::AcquireMaterialInternal(const mtl::MaterialVariantKey &key,
                                                           mtl::Material2DCreateConfig *cfg,
                                                           MaterialSpecKey *out_key)
{
    return AcquireMaterial(key, cfg, out_key, MakeInternalAccessToken());
}

MaterialTemplate *MaterialManager::AcquireMaterial(const mtl::MaterialVariantKey &key,
                                                   mtl::Material3DCreateConfig *cfg,
                                                   MaterialSpecKey *out_key,
                                                   MaterialAccessToken)
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

MaterialTemplate *MaterialManager::AcquireMaterialInternal(const mtl::MaterialVariantKey &key,
                                                           mtl::Material3DCreateConfig *cfg,
                                                           MaterialSpecKey *out_key)
{
    return AcquireMaterial(key, cfg, out_key, MakeInternalAccessToken());
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
    std::printf("[DEBUG] CreateMaterial hash=%s ST=%d GM=%d tex=%u sampler=%u\n", key_hash, int(key.GetSurfaceType()), int(key.geometry_mode), key.texture_source_bits, key.sampler_feature_bits);
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
            static_cast<unsigned>(key.GetSurfaceType()),
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

// ============================================================================
// Phase A: Slot-first API — AllocMaterialInstanceSlot
// ============================================================================

PrimitiveMaterialSlot MaterialManager::AllocMaterialInstanceSlot(
    InstanceDataDomain *domain,
    const void *instance_data,
    uint32_t instance_data_size)
{
    alloc_slot_requests.fetch_add(1);

    if (!domain)
    {
        alloc_slot_failed.fetch_add(1);
        return {};
    }

    const bool needs_mi = domain->HasLayout();
    int mi_id = -1;

    if (needs_mi)
        alloc_slot_with_mi.fetch_add(1);
    else
        alloc_slot_no_mi.fetch_add(1);

    if (needs_mi)
    {
        mi_id = domain->AllocSlot();
        if (mi_id < 0)
        {
            std::fprintf(stderr,
                "[MaterialManager] AllocMaterialInstanceSlot failed: domain requires MI but allocation failed\n");
            alloc_slot_failed.fetch_add(1);
            return {};
        }

        // Ensure deterministic first-frame MI data even when caller does not
        // provide explicit instance payload.
        if (void *mi_data = domain->GetSlotData(mi_id))
            std::memset(mi_data, 0, domain->GetDataStride());
    }

    // Write instance data if provided
    if (instance_data && instance_data_size > 0)
    {
        if (!needs_mi)
        {
            std::fprintf(stderr,
                "[MaterialManager] AllocMaterialInstanceSlot rejected payload: domain has no MI layout (size=%u)\n",
                static_cast<unsigned>(instance_data_size));
            alloc_slot_no_mi_payload_rejected.fetch_add(1);
            alloc_slot_failed.fetch_add(1);
            return {};
        }

        void *mi_data = domain->GetSlotData(mi_id);
        if (mi_data)
        {
            const uint32_t dst_bytes = domain->GetDataStride();
            const uint32_t copy_bytes = std::min(instance_data_size, dst_bytes);
            std::memcpy(mi_data, instance_data, copy_bytes);
        }
    }

    PrimitiveMaterialSlot slot;
    slot.domain       = domain;
    slot.idd_handle = idd_manager_ ? idd_manager_->GetHandle(domain) : IDDHandle{};
    slot.idd_manager   = idd_manager_;
    slot.mi_id         = mi_id;
    alloc_slot_created.fetch_add(1);
    return slot;
}

// P5: IDDHandle overload — resolves to raw ptr then delegates.
PrimitiveMaterialSlot MaterialManager::AllocMaterialInstanceSlot(
    IDDHandle idd_handle,
    const void *instance_data,
    uint32_t instance_data_size)
{
    return AllocMaterialInstanceSlot(
        idd_manager_ ? idd_manager_->Get(idd_handle) : nullptr,
        instance_data,
        instance_data_size);
}

// ============================================================================
// InstanceDataDomain — Phase 1
// ============================================================================

InstanceDataDomain *MaterialManager::CreateInstanceDataDomain(
    mtl::InstanceDataLayout layout, uint32_t max_count, uint8_t tex_array_slots)
{
    if (!idd_manager_) return nullptr;
    const IDDHandle handle = idd_manager_->Create(layout, max_count, tex_array_slots);
    return idd_manager_->Get(handle);
}

// Convenience overload: create a domain that matches the given MaterialTemplate's requirements.
// Phase E will remove this once all callsites pass layout + texslots explicitly.
InstanceDataDomain *MaterialManager::CreateInstanceDataDomain(MaterialTemplate *mtl)
{
    if(!mtl)
        return nullptr;

    return CreateInstanceDataDomain(
        mtl->GetRequiredLayout(),
        mtl->GetMIMaxCount(),
        mtl->GetTextureArraySlotFlags());
}

DomainMaterialBinding *MaterialManager::CreateDomainMaterialBinding(InstanceDataDomain *domain, MaterialTemplate *mtl)
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

    // Phase 3 / P4: register binding keyed by IDDHandle
    if (idd_manager_)
    {
        const IDDHandle h = idd_manager_->GetHandle(domain);
        if (h.IsValid())
            domain_bindings_map[h].push_back(binding);
    }

    return binding;
}

DomainMaterialBinding *MaterialManager::FindDomainMaterialBinding(IDDHandle handle, MaterialTemplate *mtl) const
{
    if (!handle.IsValid() || !mtl)
        return nullptr;

    auto it = domain_bindings_map.find(handle);
    if (it == domain_bindings_map.end())
        return nullptr;

    for (DomainMaterialBinding *binding : it->second)
    {
        if (binding && binding->GetMaterial() == mtl)
            return binding;
    }

    return nullptr;
}

// Phase 3 -- domain lifecycle management

void MaterialManager::ReleaseDomainMaterialBinding(DomainMaterialBinding *binding)
{
    if (!binding)
        return;

    InstanceDataDomain *d = binding->GetDomain();
    const IDDHandle h = idd_manager_ ? idd_manager_->GetHandle(d) : InvalidIDDHandle;
    auto it = domain_bindings_map.find(h);
    if (it != domain_bindings_map.end())
    {
        auto &vec = it->second;
        vec.erase(std::remove(vec.begin(), vec.end(), binding), vec.end());
        if (vec.empty())
            domain_bindings_map.erase(it);
    }

    delete binding;
}

void MaterialManager::ReleaseInstanceDataDomain(IDDHandle handle)
{
    if (!handle.IsValid() || !idd_manager_)
        return;

    auto it = domain_bindings_map.find(handle);
    if (it != domain_bindings_map.end())
    {
        for (auto *b : it->second)
            delete b;
        domain_bindings_map.erase(it);
    }

    idd_manager_->Release(handle);
}

void MaterialManager::ReleaseInstanceDataDomain(InstanceDataDomain *domain)
{
    if (!domain || !idd_manager_)
        return;
    ReleaseInstanceDataDomain(idd_manager_->GetHandle(domain));
}

}//namespace hgl::graph
