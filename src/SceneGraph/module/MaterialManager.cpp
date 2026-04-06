#include<hgl/graph/module/MaterialManager.h>
#include<hgl/vk/pipeline/VKGraphicsPipelineLayoutData.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKObjectNameBuilder.h>
#include<hgl/vk/VKShaderProgram.h>
#include<hgl/vk/VKMaterialInstance.h>
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
    void CreateShaderStageList(ValueArray<VkPipelineShaderStageCreateInfo> &shader_stage_list,ShaderModuleMap *shader_maps)
    {
        const ShaderModule *sm;

        const int shader_count=shader_maps->GetCount();
        shader_stage_list.Resize(shader_count);

        VkPipelineShaderStageCreateInfo *p=shader_stage_list.GetData();

        for(auto [stage, module] : *shader_maps)
        {
            sm = module;
            mem_copy(p,sm->GetCreateInfo(),1);

            ++p;
        }
    }

    bool BuildLegacyShaderModules(MaterialManager *manager,
                                  const AnsiString &mtl_name,
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
}

const ShaderModule *MaterialManager::CreateShaderModule(const AnsiString &sm_name,const ShaderCreateInfo *sci)
{
    VulkanDevice *device = GetDevice();
    if(!device)
    {
        std::fprintf(stderr, "[MaterialManager] CreateShaderModule failed: device is null for '%s'\n", sm_name.c_str());
        return(nullptr);
    }
    if(sm_name.IsEmpty())
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

    if(sm_map.Get(sm_name,sm))
        return sm;

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

    sm_map.Add(sm_name,sm);

    #ifdef _DEBUG
        {
            DebugUtils *du=device->GetDebugUtils();

            if(du)
            {
                AnsiString shader_name = "Shader:" + sm_name + AnsiString(":") + GetShaderStageName((VkShaderStageFlagBits)sci->GetShaderStage());
                du->SetShaderModule(*sm, shader_name);
            }
        }
    #endif//_DEBUG

    return sm;
}

const ShaderModule *MaterialManager::CreateShaderModuleFromSPV(const AnsiString &sm_name,
                                                                const VkShaderStageFlagBits stage,
                                                                const uint32_t *spv_data,
                                                                const size_t spv_size)
{
    VulkanDevice *device = GetDevice();
    if(!device)return(nullptr);
    if(sm_name.IsEmpty())return(nullptr);
    if(!spv_data||spv_size==0)return(nullptr);

    const int bit_offset=GetBitOffset((uint32_t)stage);

    if(bit_offset<0||bit_offset>VK_SHADER_STAGE_TYPE_COUNT)return(nullptr);

    ShaderModule *sm;

    ShaderModuleMapByName &sm_map=shader_module_by_name[bit_offset];

    if(sm_map.Get(sm_name,sm))
        return sm;

    sm=device->CreateShaderModule(stage,spv_data,spv_size);

    if(!sm)
        return(nullptr);

    sm_map.Add(sm_name,sm);

    #ifdef _DEBUG
        {
            DebugUtils *du=device->GetDebugUtils();

            if(du)
            {
                AnsiString shader_name = "Shader:" + sm_name + AnsiString(":") + GetShaderStageName(stage);
                du->SetShaderModule(*sm, shader_name);
            }
        }
    #endif//_DEBUG

    return sm;
}

GraphicsPipelineLayoutData *MaterialManager::CreateMaterialGraphicsPipelineLayoutData(const AnsiString &mtl_name, const MaterialDescriptorManager *desc_manager)
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
                du->SetPipelineLayout(pld->pipeline_layout, "PipelineLayout:" + mtl_name);
        #endif//_DEBUG
    }

    return pld;
}

MaterialParameters *MaterialManager::CreateMaterialMP(const AnsiString &mtl_name, const MaterialDescriptorManager *desc_manager, const GraphicsPipelineLayoutData *pld, const DescriptorSetType &desc_set_type)
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
                AnsiString debug_name = mtl_name + AnsiString(":") + GetDescriptorSetTypeName(desc_set_type);
                du->SetDescriptorSet(mp->GetVkDescriptorSet(), "DescSet:" + debug_name);
                du->SetDescriptorSetLayout(pld->layouts[static_cast<int>(desc_set_type)], "DescSetLayout:" + debug_name);
            }
        #endif//_DEBUG
    }

    return mp;
}

void MaterialManager::ApplyMaterialFinalizePlan(ShaderProgram *mtl, const AnsiString &mtl_name, const mtl::MaterialCreateInfo &mci)
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

    mtl->mi_data_bytes = finalize_plan.mi_data_bytes;
    mtl->mi_max_count  = finalize_plan.mi_max_count;
    // Phase 5: MI 数据池随第一次 CreateMI 时通过 default_domain 懒初始化，此处不再直接分配
}

ShaderProgram *MaterialManager::TryGetCachedMaterial(const AnsiString &name)
{
    acquire_material_cache_lookups.fetch_add(1);

    ShaderProgram *cached = nullptr;
    if(material_by_name.Get(name, cached))
    {
        acquire_material_cache_hits.fetch_add(1);
        return cached;
    }

    acquire_material_cache_misses.fetch_add(1);

    return nullptr;
}

ShaderProgram *MaterialManager::TryInitializeFallbackMaterial()
{
    if(fallback_material)
        return fallback_material;

    // Try to create a Checkerboard3D material as fallback
    // If Checkerboard3D is not available, fallback to Standard

    static const AnsiString fallback_name("__sys__fallback_checkerboard3d");

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

ShaderProgram *MaterialManager::GetFallbackMaterial()
{
    if(!fallback_material)
        TryInitializeFallbackMaterial();

    return fallback_material;
}

void MaterialManager::BindInstanceMaterial(MaterialInstance *mi, ShaderProgram *material)
{
    if(!mi)
        return;

    if(material)
        material_instance_material_map[mi] = material;
    else
        material_instance_material_map.erase(mi);
}

void MaterialManager::ForgetInstanceMaterial(MaterialInstance *mi)
{
    if(!mi)
        return;

    material_instance_material_map.erase(mi);
}

ShaderProgram *MaterialManager::ResolveMaterial(const MaterialInstance *mi) const
{
    if(!mi)
        return nullptr;

    auto it = material_instance_material_map.find(mi);
    return it != material_instance_material_map.end() ? it->second : nullptr;
}

bool MaterialManager::ExecuteMaterialBuildPipeline(ShaderProgram *mtl,
                                                   const AnsiString &mtl_name,
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

ShaderProgram *MaterialManager::CreateMaterial(const AnsiString &mtl_name,const mtl::MaterialCreateInfo *mci)
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
    const MaterialCreatePrecheckDecision precheck_decision = RunMaterialCreatePrecheck(
        mci,
        mtl_name,
        [&](const AnsiString &name)->ShaderProgram * { return TryGetCachedMaterial(name); },
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

    AutoDelete<ShaderProgram> mtl=new ShaderProgram(mtl_name,mci);
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

    material_by_name.Add(mtl_name,mtl);
    // ShaderProgram is a C++ object managed by MaterialManager, not a Vulkan object
    // No need to track with ObjectTracker
    return mtl.Finish();
}

ShaderProgram *MaterialManager::CreateMaterial(const mtl::MaterialPreset mtl_id,mtl::Material2DCreateConfig *cfg)
{
    HGL_CAPTURE_SCOPE();

    if(!cfg)
        return(nullptr);

    mtl::MaterialVariantKey key = mtl::MapPresetToVariantKey(mtl_id);
    mtl::ApplyCreateConfigToVariantKey(key, cfg);
    return CreateMaterial(key, cfg);
}

ShaderProgram *MaterialManager::AcquireMaterial(const MaterialSpec &spec, MaterialSpecKey *out_key)
{
    if(!spec.IsValid())
        return nullptr;

    ShaderProgram *result = nullptr;

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

ShaderProgram *MaterialManager::AcquireMaterial(const mtl::MaterialPreset mtl_id, mtl::Material2DCreateConfig *cfg, MaterialSpecKey *out_key)
{
    acquire_material_requests.fetch_add(1);

    ShaderProgram *mtl = CreateMaterial(mtl_id, cfg);

    if(!mtl)
    {
        mtl = GetFallbackMaterial();
        if(mtl)
            acquire_fallback_used.fetch_add(1);
    }

    if(out_key)
        out_key->cache_name = mtl ? mtl->GetName() : AnsiString();

    return mtl;
}

ShaderProgram *MaterialManager::AcquireMaterial(const mtl::MaterialPreset mtl_id, mtl::Material3DCreateConfig *cfg, MaterialSpecKey *out_key)
{
    acquire_material_requests.fetch_add(1);

    ShaderProgram *mtl = CreateMaterial(mtl_id, cfg);

    if(!mtl)
    {
        mtl = GetFallbackMaterial();
        if(mtl)
            acquire_fallback_used.fetch_add(1);
    }

    if(out_key)
        out_key->cache_name = mtl ? mtl->GetName() : AnsiString();

    return mtl;
}

ShaderProgram *MaterialManager::AcquireMaterial(const mtl::MaterialVariantKey &key, mtl::Material2DCreateConfig *cfg, MaterialSpecKey *out_key)
{
    acquire_material_requests.fetch_add(1);

    ShaderProgram *mtl = CreateMaterial(key, cfg);

    if(!mtl)
    {
        mtl = GetFallbackMaterial();
        if(mtl)
            acquire_fallback_used.fetch_add(1);
    }

    if(out_key)
        out_key->cache_name = mtl ? mtl->GetName() : AnsiString();

    return mtl;
}

ShaderProgram *MaterialManager::AcquireMaterial(const mtl::MaterialVariantKey &key, mtl::Material3DCreateConfig *cfg, MaterialSpecKey *out_key)
{
    acquire_material_requests.fetch_add(1);

    ShaderProgram *mtl = CreateMaterial(key, cfg);

    if(!mtl)
    {
        mtl = GetFallbackMaterial();
        if(mtl)
            acquire_fallback_used.fetch_add(1);
    }

    if(out_key)
        out_key->cache_name = mtl ? mtl->GetName() : AnsiString();

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

ShaderProgram *MaterialManager::CreateMaterial(const mtl::MaterialPreset mtl_id,mtl::Material3DCreateConfig *cfg)
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

ShaderProgram *MaterialManager::CreateMaterial(const mtl::MaterialVariantKey &key,mtl::Material2DCreateConfig *cfg)
{
    HGL_CAPTURE_SCOPE();

    if(!cfg)
        return(nullptr);

    AnsiString hash_name="variant";
    char key_hash[32] = {};
    std::snprintf(key_hash, sizeof(key_hash), "%llu", static_cast<unsigned long long>(key.Hash()));
    std::printf("[DEBUG] CreateMaterial hash=%s ST=%d GM=%d tex=%u sampler=%u\n", key_hash, int(key.surface_type), int(key.geometry_mode), key.texture_source_bits, key.sampler_feature_bits);
    hash_name+="#";
    hash_name+=key_hash;
    hash_name+="?";
    hash_name+=cfg->ToHashStdString().c_str();

    {
        ShaderProgram *cached = TryGetCachedMaterial(hash_name);
        if (cached)
            return cached;
    }

    const auto *profile=GetPhysicalDeviceProfile();

    AutoDelete<mtl::MaterialCreateInfo> mci=mtl::CreateMaterialCreateInfo(profile,key,cfg);

    if(!mci)
        return(nullptr);

    ShaderProgram *mat = this->CreateMaterial(hash_name,mci);
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

ShaderProgram *MaterialManager::CreateMaterial(const mtl::MaterialVariantKey &key,mtl::Material3DCreateConfig *cfg)
{
    HGL_CAPTURE_SCOPE();

    if(!cfg)
    {
        std::fprintf(stderr, "[MaterialManager] CreateMaterial(key/3D) failed: cfg is null\n");
        return(nullptr);
    }

    AnsiString hash_name="variant";
    char key_hash[32] = {};
    std::snprintf(key_hash, sizeof(key_hash), "%llu", static_cast<unsigned long long>(key.Hash()));
    hash_name+="#";
    hash_name+=key_hash;
    hash_name+="?";
    hash_name+=cfg->ToHashStdString().c_str();

    {
        ShaderProgram *cached = TryGetCachedMaterial(hash_name);
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

    ShaderProgram *mat = this->CreateMaterial(hash_name,mci);
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
        ShaderProgram *mtl = spec.material;
        if(!mtl) return nullptr;

        if(spec.vil_cfg)
        {
            mi = mtl->CreateMI(this, spec.vil_cfg);
            if(mi)
            {
                Add(mi);
                BindInstanceMaterial(mi, mtl);
                if(spec.instance_data && spec.instance_data_size > 0)
                    mi->WriteMIData(spec.instance_data, spec.instance_data_size);
            }
        }
        else
        {
            mi = mtl->CreateMI(this, spec.vil);
            if(mi)
            {
                Add(mi);
                BindInstanceMaterial(mi, mtl);
                VulkanDevice *device = GetDevice();
                if(device)
                    device->TrackObject(VK_OBJECT_TYPE_UNKNOWN, (uint64_t)(uintptr_t)mi,
                                      ObjectNameBuilder(mtl->GetName()).Append(ObjectTypeTag::MaterialInstance));
                if(spec.instance_data && spec.instance_data_size > 0)
                    mi->WriteMIData(spec.instance_data, spec.instance_data_size);
            }
        }
    }

    if(!mi)
        return nullptr;

    acquire_mi_created.fetch_add(1);

    mi->SetRenderPreset(spec.preset);

    if(out_key)
    {
        out_key->material = mi->GetMaterial();
        out_key->vil = mi->GetVIL();
        out_key->preset = mi->GetRenderPreset();
        out_key->domain = mi->GetDomain();
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
// MaterialResourceDomain — Phase 1
// ============================================================================

MaterialResourceDomain *MaterialManager::CreateMaterialResourceDomain(ShaderProgram *mtl)
{
    if(!mtl)
        return nullptr;

    return CreateMaterialResourceDomain(mtl->GetMIDataBytes(), mtl->GetMIMaxCount());
}

MaterialResourceDomain *MaterialManager::CreateMaterialResourceDomain(uint32_t mi_data_bytes,
                                                      uint32_t mi_max_count)
{
    return new MaterialResourceDomain(mi_data_bytes, mi_max_count);
}

DomainMaterialBinding *MaterialManager::CreateDomainMaterialBinding(MaterialResourceDomain *domain, ShaderProgram *mtl)
{
    if (!domain || !mtl)
        return nullptr;

    // Hard reject: MI stride mismatch means MI data cannot be shared
    if (domain->GetMIDataBytes() != mtl->GetMIDataBytes())
    {
        std::fprintf(stderr,
            "[MaterialManager] CreateDomainMaterialBinding: MI stride mismatch "
            "domain=%u mtl=%u\n",
            domain->GetMIDataBytes(), mtl->GetMIDataBytes());
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
                                                          ShaderProgram *material,
                                                          const VIL *vil,
                                                          const void *data,
                                                          const uint32 data_size)
{
    if(!domain || !material)
        return nullptr;

    const VIL *use_vil = vil ? vil : material->GetDefaultVIL();
    int mi_id = domain->AllocMISlot();

    MaterialInstance *mi = new MaterialInstance(this, domain, use_vil, mi_id);
    mi->InitMITLayout(material->GetTextureArraySlotFlags());
    Add(mi);
    BindInstanceMaterial(mi, material);

    if(data && data_size > 0)
        mi->WriteMIData(data, data_size);

    return mi;
}

bool MaterialManager::RebindMaterialInstance(MaterialInstance *mi, ShaderProgram *material, const VIL *vil)
{
    if(!mi || !material)
        return false;

    BindInstanceMaterial(mi, material);
    mi->vil = vil ? vil : material->GetDefaultVIL();
    mi->InitMITLayout(material->GetTextureArraySlotFlags());
    return true;
}

}//namespace hgl::graph
