#include<hgl/graph/module/MaterialManager.h>
#include<hgl/vk/pipeline/VKPipelineLayoutData.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKObjectNameBuilder.h>
#include<hgl/vk/VKMaterialProgram.h>
#include<hgl/vk/VKMaterialInstance.h>
#include<hgl/vk/VKMaterialParameters.h>
#include<hgl/vk/VKShaderModule.h>
#include<hgl/vk/VKShaderModuleMap.h>
#include<hgl/vk/VKMaterialDescriptorManager.h>
#include<hgl/vk/VKVertexInput.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/MaterialCreatePrecheckAdapter.h>
#include<hgl/graph/module/MaterialFinalizeFlowAdapter.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/ShaderCreateInfoVertex.h>
#include<hgl/mtl/Material2DCreateConfig.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/object/ObjectTracker.h>
#include<cstdint>
#include<vector>

namespace hgl::graph{

namespace
{
    class ScopedMaterialGeometryBinding
    {
    private:
        mtl::MaterialCreateConfig *cfg_=nullptr;
        const GeometryVertexFormat *prev_=nullptr;

    public:
        ScopedMaterialGeometryBinding(mtl::MaterialCreateConfig *cfg,const GeometryVertexFormat *gvf):cfg_(cfg)
        {
            if(cfg_)
            {
                prev_=cfg_->geometry_vertex_format;
                cfg_->SetGeometryVertexFormat(gvf);
            }
        }

        ~ScopedMaterialGeometryBinding()
        {
            if(cfg_)
                cfg_->SetGeometryVertexFormat(prev_);
        }
    };

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
                                  const ShaderCreateInfoMap &sci_map,
                                  ShaderModuleMap *shader_maps)
    {
        if (!manager || !shader_maps)
            return false;

        if (sci_map.GetCount() < 2)
            return false;

        for (auto [stage, sci_ptr] : sci_map)
        {
            (void)stage;

            if (!sci_ptr)
                return false;

            const ShaderModule *module = manager->CreateShaderModule(mtl_name, sci_ptr);
            if (!module)
                return false;

            shader_maps->Add(module);
        }

        return true;
    }

    std::vector<ShaderDescriptor> CollectLegacyDescriptors(const mtl::MaterialCreateInfo *mci)
    {
        std::vector<ShaderDescriptor> legacy_descriptors;
        if (!mci)
            return legacy_descriptors;

        const auto &mdi = mci->GetDescriptorInfo();
        if (mdi.GetCount() == 0)
            return legacy_descriptors;

        const auto &sds_array = mdi.Get();
        legacy_descriptors.reserve(mdi.GetCount());

        for (size_t i = 0; i < DESCRIPTOR_SET_TYPE_COUNT; i++)
        {
            std::vector<ShaderDescriptor *> values;
            sds_array[i].descriptor_map.GetValueArray(values);

            for (auto *sd : values)
            {
                if (sd)
                    legacy_descriptors.emplace_back(*sd);
            }
        }

        return legacy_descriptors;
    }

}//namespace

GRAPH_MODULE_CONSTRUCT(MaterialManager)
{
}

const ShaderModule *MaterialManager::CreateShaderModule(const AnsiString &sm_name,const ShaderCreateInfo *sci)
{
    VulkanDevice *device = GetDevice();
    if(!device)return(nullptr);
    if(sm_name.IsEmpty())return(nullptr);

    const int bit_offset=GetBitOffset((uint32_t)sci->GetShaderStage());

    if(bit_offset<0||bit_offset>VK_SHADER_STAGE_TYPE_COUNT)return(nullptr);

    ShaderModule *sm;

    ShaderModuleMapByName &sm_map=shader_module_by_name[bit_offset];

    if(sm_map.Get(sm_name,sm))
        return sm;

    sm=device->CreateShaderModule((VkShaderStageFlagBits)sci->GetShaderStage(),sci->GetSPVData(),sci->GetSPVSize());

    if(!sm)
        return(nullptr);

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

PipelineLayoutData *MaterialManager::CreateMaterialPipelineLayoutData(const AnsiString &mtl_name, const MaterialDescriptorManager *desc_manager)
{
    VulkanDevice *device = GetDevice();
    if(!device) return nullptr;

    PipelineLayoutData *pld = device->CreatePipelineLayoutData(desc_manager, bindless_layout_);

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

MaterialParameters *MaterialManager::CreateMaterialMP(const AnsiString &mtl_name, const MaterialDescriptorManager *desc_manager, const PipelineLayoutData *pld, const DescriptorSetType &desc_set_type)
{
    VulkanDevice *device = GetDevice();
    if(!device) return nullptr;

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

void MaterialManager::ApplyMaterialFinalizePlan(MaterialProgram *mtl, const AnsiString &mtl_name, const mtl::MaterialCreateInfo &mci)
{
    if(!mtl)
        return;

    MaterialFinalizePlan finalize_plan;
    BuildMaterialFinalizePlan(mtl->desc_manager, mci, finalize_plan);

    mtl->pipeline_layout_data = CreateMaterialPipelineLayoutData(mtl_name, mtl->desc_manager);

    for(const auto set_type : finalize_plan.mp_set_types)
    {
        mtl->mp_array[(int)set_type] = CreateMaterialMP(mtl_name, mtl->desc_manager, mtl->pipeline_layout_data, set_type);
    }

    mtl->mi_data_bytes = finalize_plan.mi_data_bytes;
    mtl->mi_max_count  = finalize_plan.mi_max_count;
}

MaterialProgram *MaterialManager::TryGetCachedMaterial(const AnsiString &name)
{
    MaterialProgram *cached = nullptr;
    if(material_by_name.Get(name, cached))
        return cached;

    return nullptr;
}

bool MaterialManager::ExecuteMaterialBuildPipeline(MaterialProgram *mtl,
                                                   const AnsiString &mtl_name,
                                                   const mtl::MaterialCreateInfo *mci,
                                                   const ShaderCreateInfoMap &sci_map)
{
    if(!mtl || !mci)
        return false;

    if(!BuildLegacyShaderModules(this,
                                 mtl_name,
                                 sci_map,
                                 mtl->shader_maps))
    {
        return false;
    }

    CreateShaderStageList(mtl->shader_stage_list,mtl->shader_maps);

    const ShaderCreateInfoVertex *vert = mci->GetVertexShader();
    mtl->vertex_input = vert ? GetVertexInput(vert->GetInput()) : nullptr;

    std::vector<ShaderDescriptor> descriptors = CollectLegacyDescriptors(mci);
    if(!descriptors.empty())
        mtl->desc_manager = new MaterialDescriptorManager(mtl_name, descriptors.data(), static_cast<uint>(descriptors.size()));
    else
        mtl->desc_manager = nullptr;

    ApplyMaterialFinalizePlan(mtl, mtl_name, *mci);

    return true;
}

MaterialProgram *MaterialManager::AcquireMaterialProgram(const AnsiString &mtl_name,const mtl::MaterialCreateInfo *mci)
{
    HGL_CAPTURE_SCOPE();

    if(!mci)
        return(nullptr);

    MaterialCreatePrecheckResult precheck_result;
    const MaterialCreatePrecheckDecision precheck_decision = RunMaterialCreatePrecheck(
        mci,
        mtl_name,
        [&](const AnsiString &name)->MaterialProgram * { return TryGetCachedMaterial(name); },
        precheck_result);

    if(precheck_decision == MaterialCreatePrecheckDecision::UseCached)
        return precheck_result.cached_material;

    if(precheck_decision != MaterialCreatePrecheckDecision::Proceed)
        return nullptr;

    const ShaderCreateInfoMap &sci_map = *precheck_result.shader_map;

    AutoDelete<MaterialProgram> mtl=new MaterialProgram(mtl_name,mci);
    if(!ExecuteMaterialBuildPipeline(mtl,
                                     mtl_name,
                                     mci,
                                     sci_map))
        return nullptr;

    Add(mtl);

    material_by_name.Add(mtl_name,mtl);
    // MaterialProgram is a C++ object managed by MaterialManager, not a Vulkan object
    // No need to track with ObjectTracker
    return mtl.Finish();
}

MaterialProgram *MaterialManager::AcquireMaterialProgram(const mtl::MaterialPreset mtl_id,mtl::Material2DCreateConfig *cfg)
{
    HGL_CAPTURE_SCOPE();

    if(!cfg)
        return(nullptr);

    const auto *profile=GetPhysicalDeviceProfile();

    AutoDelete<mtl::MaterialCreateInfo> mci=mtl::CreateMaterialCreateInfo(profile,mtl_id,cfg);

    if(!mci)
        return(nullptr);

    AnsiString hash_name=mtl::GetMaterialPresetName(mtl_id);
    hash_name+="?";
    hash_name+=cfg->ToHashStdString().c_str();

    return this->AcquireMaterialProgram(hash_name,mci);
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

MaterialProgram *MaterialManager::AcquireMaterialProgram(const mtl::MaterialPreset mtl_id,mtl::Material3DCreateConfig *cfg)
{
    HGL_CAPTURE_SCOPE();

    if(!cfg)
        return(nullptr);

    const auto *profile=GetPhysicalDeviceProfile();

    AutoDelete<mtl::MaterialCreateInfo> mci=mtl::CreateMaterialCreateInfo(profile,mtl_id,cfg);

    if(!mci)
        return(nullptr);

    AnsiString hash_name=mtl::GetMaterialPresetName(mtl_id);
    hash_name+="?";
    hash_name+=cfg->ToHashStdString().c_str();

    return this->AcquireMaterialProgram(hash_name,mci);
}

MaterialProgram *MaterialManager::AcquireMaterialProgramByBMI(const std::string &bmi_id,
                                                               const mtl::MaterialRecipe &recipe,
                                                               PrimitiveType prim_type,
                                                               const GeometryVertexFormat *geometry_vertex_format)
{
    // 1. 查询 BMI：按 bmi_id 主键；失败则用 3D 通用保底
    mtl::BaseMaterialInfo bmi{};
    bool has_bmi = mtl::TryGetBaseMaterialInfoByBMIId(bmi_id, bmi);

    if (!has_bmi)
        has_bmi = mtl::TryGetBaseMaterialInfoByBMIId(mtl::BUILTIN_BMI_FALLBACK_3D, bmi);

    if (!has_bmi)
        return nullptr;

    // 2. Part-C: 解析 preset（当前阶段映射到具体 M_* 创建函数）
    if (bmi.preset_hint == mtl::InvalidMaterialPresetHint
     || bmi.preset_hint >= static_cast<uint32_t>(mtl::MaterialPreset::RANGE_SIZE))
        return nullptr;

    const mtl::MaterialPreset preset = static_cast<mtl::MaterialPreset>(bmi.preset_hint);

    // 3. Part-C: 从 BMI 字段直接读取所需配置——无任何推断
    if (bmi.is_text)
    {
        mtl::Text2DMaterialCreateConfig cfg;
        cfg.prim = prim_type;
        if (geometry_vertex_format)
        {
            ScopedMaterialGeometryBinding scoped(&cfg, geometry_vertex_format);
            return AcquireMaterialProgram(preset, &cfg);
        }
        return AcquireMaterialProgram(preset, &cfg);
    }

    if (bmi.is_2d)
    {
        const mtl::WithLocalToWorld with_l2w =
            recipe.local_to_world_2d ? mtl::WithLocalToWorld::With : mtl::WithLocalToWorld::Without;
        mtl::Material2DCreateConfig cfg(prim_type, recipe.coordinate_system_2d, with_l2w);
        if (geometry_vertex_format)
        {
            ScopedMaterialGeometryBinding scoped(&cfg, geometry_vertex_format);
            return AcquireMaterialProgram(preset, &cfg);
        }
        return AcquireMaterialProgram(preset, &cfg);
    }

    {
        // 3D（包括 Sky 材质）：完全由 BMI 字段驱动，无任何推断
        const mtl::WithCamera      wc = bmi.with_camera        ? mtl::WithCamera::With      : mtl::WithCamera::Without;
        const mtl::WithLocalToWorld wl = bmi.with_local_to_world ? mtl::WithLocalToWorld::With : mtl::WithLocalToWorld::Without;
        const mtl::WithSky         ws = bmi.with_sky           ? mtl::WithSky::With         : mtl::WithSky::Without;

        mtl::Material3DCreateConfig cfg(prim_type, wc, wl, ws);
        if (geometry_vertex_format)
        {
            ScopedMaterialGeometryBinding scoped(&cfg, geometry_vertex_format);
            return AcquireMaterialProgram(preset, &cfg);
        }
        return AcquireMaterialProgram(preset, &cfg);
    }
}

MaterialInstance *MaterialManager::CreateMaterialInstanceInternal(MaterialProgram *mtl)
{
    HGL_CAPTURE_SCOPE();

    if(!mtl)return(nullptr);

    MaterialInstance *mi=mtl->CreateMI();

    if(mi)
    {
        Add(mi);
        VulkanDevice *device = GetDevice();
        if(device)
            device->TrackObject(VK_OBJECT_TYPE_UNKNOWN, (uint64_t)(uintptr_t)mi,
                              ObjectNameBuilder(mtl->GetName()).Append(ObjectTypeTag::MaterialInstance));
    }

    return mi;
}

MaterialInstance *MaterialManager::CreateMaterialInstanceInternal(MaterialProgram *mtl,const VIL *vil)
{
    HGL_CAPTURE_SCOPE();

    if(!mtl)return(nullptr);

    MaterialInstance *mi=mtl->CreateMI(vil);

    if(mi)
    {
        Add(mi);
        VulkanDevice *device = GetDevice();
        if(device)
            device->TrackObject(VK_OBJECT_TYPE_UNKNOWN, (uint64_t)(uintptr_t)mi,
                              ObjectNameBuilder(mtl->GetName()).Append(ObjectTypeTag::MaterialInstance));
    }

    return mi;
}

MaterialInstance *MaterialManager::CreateMaterialInstanceInternal(MaterialProgram *mtl,const VILConfig *vil_cfg)
{
    HGL_CAPTURE_SCOPE();

    if(!mtl)return(nullptr);

    MaterialInstance *mi=mtl->CreateMI(vil_cfg);

    if(mi)
    {
        Add(mi);
        VulkanDevice *device = GetDevice();
        if(device)
            device->TrackObject(VK_OBJECT_TYPE_UNKNOWN, (uint64_t)(uintptr_t)mi,
                              ObjectNameBuilder(mtl->GetName()).Append(ObjectTypeTag::MaterialInstance));
    }

    return mi;
}

MaterialInstance *MaterialManager::CreateMaterialInstanceInternal(MaterialProgram *mtl,const GeometryVertexFormat &geometry_vertex_format)
{
    return CreateMaterialInstanceInternal(mtl,geometry_vertex_format,nullptr,0);
}

MaterialInstance *MaterialManager::CreateMaterialInstanceInternal(MaterialProgram *mtl,const GeometryVertexFormat &geometry_vertex_format,const void *mi_data,const uint32 mi_bytes)
{
    HGL_CAPTURE_SCOPE();

    if(!mtl)return(nullptr);

    MaterialInstance *mi=mtl->CreateMI(geometry_vertex_format);

    if(mi)
    {
        Add(mi);
        VulkanDevice *device = GetDevice();
        if(device)
            device->TrackObject(VK_OBJECT_TYPE_UNKNOWN, (uint64_t)(uintptr_t)mi,
                              ObjectNameBuilder(mtl->GetName()).Append(ObjectTypeTag::MaterialInstance));
    }

    if(mi_data&&mi_bytes>0)
    {
        // WriteMIData is deprecated in the new path (data lives in external SSBO).
        // Legacy callers that still pass data will silently have it dropped here.
    }

    return mi;
}

MaterialInstance *MaterialManager::CreateMaterialInstanceInternal(MaterialProgram *mtl,const VIL *vil,const void *mi_data,const uint32 mi_bytes)
{
    HGL_CAPTURE_SCOPE();

    if(!mtl)return(nullptr);

    MaterialInstance *mi=mtl->CreateMI(vil);

    if(!mi)
        return nullptr;

    Add(mi);
    VulkanDevice *device = GetDevice();
    if(device)
        device->TrackObject(VK_OBJECT_TYPE_UNKNOWN, (uint64_t)(uintptr_t)mi,
                          ObjectNameBuilder(mtl->GetName()).Append(ObjectTypeTag::MaterialInstance));

    if(mi_data&&mi_bytes>0)
    {
        // WriteMIData is deprecated in the new path (data lives in external SSBO).
        // Legacy callers that still pass data will silently have it dropped here.
        // Migrate to DescriptorBindingSet + external SSBO for correct behavior.
    }

    return mi;
}

MaterialInstance *MaterialManager::CreateMaterialInstanceInternal(MaterialProgram *mtl,const VILConfig *vil_cfg,const void *mi_data,const uint32 mi_bytes)
{
    HGL_CAPTURE_SCOPE();

    if(!mtl)return(nullptr);

    MaterialInstance *mi=mtl->CreateMI(vil_cfg);

    if(!mi)
        return nullptr;

    Add(mi);
    // MaterialInstance is a C++ object managed by MaterialManager, not a Vulkan object
    // No need to track with ObjectTracker

    if(mi_data&&mi_bytes>0)
    {
        // WriteMIData is deprecated — silently dropped.
    }

    return mi;
}

MaterialInstance *MaterialManager::CreateMaterialInstanceInternal(const mtl::MaterialPreset mtl_id,mtl::Material2DCreateConfig *mcc,const VILConfig *vil_cfg,const void *data,const uint32 data_size)
{
    HGL_CAPTURE_SCOPE();

    MaterialProgram *mtl=this->AcquireMaterialProgram(mtl_id,mcc);

    if(!mtl)
        return(nullptr);

    return CreateMaterialInstanceInternal(mtl,vil_cfg,data,data_size);
}

MaterialInstance *MaterialManager::CreateMaterialInstanceInternal(const mtl::MaterialPreset mtl_id,mtl::Material3DCreateConfig *mcc,const VILConfig *vil_cfg,const void *data,const uint32 data_size)
{
    HGL_CAPTURE_SCOPE();

    MaterialProgram *mtl=this->AcquireMaterialProgram(mtl_id,mcc);

    if(!mtl)
        return(nullptr);

    return CreateMaterialInstanceInternal(mtl,vil_cfg,data,data_size);
}

MaterialInstance *MaterialManager::CreateMaterialInstanceInternal(const mtl::MaterialPreset mtl_id,mtl::Material2DCreateConfig *mcc,const GeometryVertexFormat &geometry_vertex_format,const void *data,const uint32 data_size)
{
    HGL_CAPTURE_SCOPE();

    if(!mcc)
        return nullptr;

    ScopedMaterialGeometryBinding scoped(mcc, &geometry_vertex_format);
    MaterialProgram *mtl=this->AcquireMaterialProgram(mtl_id,mcc);

    if(!mtl)
        return(nullptr);

    return CreateMaterialInstanceInternal(mtl,geometry_vertex_format,data,data_size);
}

MaterialInstance *MaterialManager::CreateMaterialInstanceInternal(const mtl::MaterialPreset mtl_id,mtl::Material3DCreateConfig *mcc,const GeometryVertexFormat &geometry_vertex_format,const void *data,const uint32 data_size)
{
    HGL_CAPTURE_SCOPE();

    if(!mcc)
        return nullptr;

    ScopedMaterialGeometryBinding scoped(mcc, &geometry_vertex_format);
    MaterialProgram *mtl=this->AcquireMaterialProgram(mtl_id,mcc);

    if(!mtl)
        return(nullptr);

    return CreateMaterialInstanceInternal(mtl,geometry_vertex_format,data,data_size);
}

}//namespace hgl::graph
