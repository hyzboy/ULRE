#include<hgl/graph/module/MaterialManager.h>
#include<hgl/vk/pipeline/VKPipelineLayoutData.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKObjectNameBuilder.h>
#include<hgl/vk/VKShaderProgram.h>
#include<hgl/vk/VKMaterialParameters.h>
#include<hgl/vk/VKShaderModule.h>
#include<hgl/vk/VKShaderModuleMap.h>
#include<hgl/vk/VKMaterialDescriptorManager.h>
#include<hgl/vk/VKVertexInput.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/MaterialCreatePrecheckAdapter.h>
#include<hgl/graph/module/MaterialFinalizeFlowAdapter.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>
#include<hgl/shadergen/ShaderProgramBuildSpec.h>
#include<hgl/shadergen/ShaderCreateInfoVertex.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/object/ObjectTracker.h>
#include<cstdint>
#include<vector>

namespace hgl::graph{

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

    std::vector<ShaderDescriptor> CollectLegacyDescriptors(const mtl::ShaderProgramBuildSpec *mci)
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

void MaterialManager::ApplyMaterialFinalizePlan(ShaderProgram *mtl, const AnsiString &mtl_name, const mtl::ShaderProgramBuildSpec &mci)
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

}

ShaderProgram *MaterialManager::TryGetCachedMaterial(const AnsiString &name)
{
    ShaderProgram *cached = nullptr;
    if(material_by_name.Get(name, cached))
        return cached;

    return nullptr;
}

bool MaterialManager::ExecuteMaterialBuildPipeline(ShaderProgram *mtl,
                                                   const AnsiString &mtl_name,
                                                   const mtl::ShaderProgramBuildSpec *mci,
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

ShaderProgram *MaterialManager::AcquireMaterialProgram(const AnsiString &mtl_name,const mtl::ShaderProgramBuildSpec *mci)
{
    HGL_CAPTURE_SCOPE();

    if(!mci)
        return(nullptr);

    MaterialCreatePrecheckResult precheck_result;
    const MaterialCreatePrecheckDecision precheck_decision = RunMaterialCreatePrecheck(
        mci,
        mtl_name,
        [&](const AnsiString &name)->ShaderProgram * { return TryGetCachedMaterial(name); },
        precheck_result);

    if(precheck_decision == MaterialCreatePrecheckDecision::UseCached)
        return precheck_result.cached_material;

    if(precheck_decision != MaterialCreatePrecheckDecision::Proceed)
        return nullptr;

    const ShaderCreateInfoMap &sci_map = *precheck_result.shader_map;

    AutoDelete<ShaderProgram> mtl=new ShaderProgram(mtl_name,mci);
    if(!ExecuteMaterialBuildPipeline(mtl,
                                     mtl_name,
                                     mci,
                                     sci_map))
        return nullptr;

    Add(mtl);

    material_by_name.Add(mtl_name,mtl);
    // ShaderProgram is a C++ object managed by MaterialManager, not a Vulkan object
    // No need to track with ObjectTracker
    return mtl.Finish();
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

ShaderProgram *MaterialManager::AcquireMaterialProgram(const std::string &mtl_def_id,
                                                               const mtl::MaterialRecipe &recipe,
                                                               PrimitiveType prim_type,
                                                               const GeometryVertexFormat *geometry_vertex_format)
{
    mtl::MaterialDefinitionBuildRequest request{};
    request.mtl_def_id = mtl_def_id;
    request.recipe = recipe;
    request.primitive_type = prim_type;
    request.geometry_vertex_format = geometry_vertex_format;
    return AcquireMaterialProgram(request);
}

ShaderProgram *MaterialManager::AcquireMaterialProgram(const mtl::MaterialDefinitionBuildRequest &request)
{
    const std::string &mtl_def_id = request.mtl_def_id;

    // 1. 查询 BMI：按 mtl_def_id 主键；失败时按几何维度选择 2D/3D 保底
    mtl::MaterialDefinition bmi{};
    bool has_bmi = mtl::TryGetMaterialDefinitionByID(mtl_def_id, bmi);

    if (!has_bmi)
    {
        const char *fallback_def_id = mtl::GetFallbackMaterialDefinitionID(mtl::ShouldUse2DFallbackMaterial(request));
        has_bmi = mtl::TryGetMaterialDefinitionByID(fallback_def_id, bmi);
    }

    if (!has_bmi)
        return nullptr;

    // 2. Part-C: 解析内置 creator（当前阶段映射到具体 M_* 创建函数）
    if (bmi.builtin_creator_id == mtl::InvalidBuiltinMaterialCreatorIDHint
     || bmi.builtin_creator_id >= static_cast<uint32_t>(mtl::BuiltinMaterialCreatorID::RANGE_SIZE))
        return nullptr;

    const mtl::BuiltinMaterialCreatorID preset = static_cast<mtl::BuiltinMaterialCreatorID>(bmi.builtin_creator_id);
    const auto *profile = GetPhysicalDeviceProfile();
    AutoDelete<mtl::ShaderProgramBuildSpec> mci = mtl::CreateMaterialCreateInfo(profile, preset, bmi, request);
    if(!mci)
        return nullptr;

    const std::string hash_std = mtl::BuildBuiltinMaterialCreatorRequestHash(preset, bmi, request);
    AnsiString hash_name = hash_std.c_str();
    return this->AcquireMaterialProgram(hash_name, mci);
}

}//namespace hgl::graph
