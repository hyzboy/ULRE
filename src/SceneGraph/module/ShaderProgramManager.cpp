#include<hgl/graph/module/ShaderProgramManager.h>
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
#include<hgl/graph/module/ShaderProgramCreatePrecheckAdapter.h>
#include<hgl/graph/module/ShaderProgramFinalizeFlowAdapter.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>
#include<hgl/shadergen/ShaderProgramBuildSpec.h>
#include<hgl/shadergen/ShaderArtifactStore.h>
#include<hgl/shadergen/ShaderCreateInfoVertex.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/mtl/MaterialDefinitionFile.h>
#include<hgl/object/ObjectTracker.h>
#include<cstdint>
#include<vector>

namespace hgl::graph{

namespace
{
    bool ResolveMaterialDefinitionForRequest(const mtl::MaterialDefinitionBuildRequest &request,
                                             const mtl::MaterialDefinitionFileRegistry *file_registry,
                                             mtl::MaterialDefinition &out_bmi)
    {
        const std::string &mtl_def_id = request.recipe.mtl_def_id;

        bool has_bmi = mtl::TryGetMaterialDefinitionByID(mtl_def_id, out_bmi);
        if (has_bmi && file_registry && !mtl::IsBootstrapMaterialDefinition(out_bmi))
        {
            const mtl::MaterialDefinition *file_definition =
                file_registry->FindByID(mtl_def_id.c_str());
            mtl::MaterialDefinition merged;
            if (file_definition
             && mtl::MergeMaterialDefinitionFile(out_bmi, *file_definition, merged))
                out_bmi = merged;
        }
        if (!has_bmi)
        {
            const char *fallback_def_id = mtl::GetFallbackMaterialDefinitionID(mtl::ShouldUse2DFallbackMaterial(request));
            has_bmi = mtl::TryGetMaterialDefinitionByID(fallback_def_id, out_bmi);
        }

        if (!has_bmi)
            return false;

        return true;
    }

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

    bool BuildShaderModulesFromCreateInfoMap(ShaderProgramManager *manager,
                                             const AnsiString &mtl_name,
                                             const ShaderCreateInfoMap &sci_map,
                                             const mtl::ShaderProgramBuildSpec *build_spec,
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

            const ShaderModule *module = nullptr;
            mtl::ShaderArtifactStore *artifact_store =
                build_spec ? build_spec->GetArtifactStore() : nullptr;
            const mtl::ShaderStageKey *cache_key = nullptr;
            if (build_spec && build_spec->HasProgramLink())
            {
                const auto &link = build_spec->GetProgramLink();
                cache_key = stage == ShaderStage::Vertex
                    ? &link.vertex_stage : stage == ShaderStage::Fragment
                        ? &link.fragment_stage : nullptr;
            }

            if (artifact_store && cache_key)
            {
                hgl::ValueArray<hgl::uint8> cached_spv;
                if (artifact_store->LoadStageSPV(*cache_key, cached_spv))
                {
                    module = manager->CreateShaderModuleFromSPV(
                        *cache_key,
                        reinterpret_cast<const uint32_t *>(cached_spv.GetData()),
                        static_cast<size_t>(cached_spv.GetCount()));
                }
            }

            if (!module)
                module = manager->CreateShaderModule(mtl_name, sci_ptr);
            if (!module)
                return false;

            shader_maps->Add(module);
        }

        return true;
    }

    std::vector<ShaderDescriptor> CollectDescriptorsFromBuildSpec(const mtl::ShaderProgramBuildSpec *mci)
    {
        std::vector<ShaderDescriptor> descriptors;
        if (!mci)
            return descriptors;

        const auto &mdi = mci->GetDescriptorInfo();
        if (mdi.GetCount() == 0)
            return descriptors;

        const auto &sds_array = mdi.Get();
        descriptors.reserve(mdi.GetCount());

        for (size_t i = 0; i < DESCRIPTOR_SET_TYPE_COUNT; i++)
        {
            std::vector<ShaderDescriptor *> values;
            sds_array[i].descriptor_map.GetValueArray(values);

            for (auto *sd : values)
            {
                if (sd)
                    descriptors.emplace_back(*sd);
            }
        }

        return descriptors;
    }

}//namespace

GRAPH_MODULE_CONSTRUCT(ShaderProgramManager)
{
    (void)mtl::GetMaterialDefinitionFileRegistry();
}

const ShaderModule *ShaderProgramManager::CreateShaderModule(const AnsiString &sm_name,const ShaderCreateInfo *sci)
{
    VulkanDevice *device = GetDevice();
    if(!device)return(nullptr);
    if(sm_name.IsEmpty())return(nullptr);

    ShaderModule *sm = shader_module_cache.FindName(sci->GetShaderStage(), sm_name);
    if(sm)
        return sm;

    sm=device->CreateShaderModule((VkShaderStageFlagBits)sci->GetShaderStage(),sci->GetSPVData(),sci->GetSPVSize());

    if(!sm)
        return(nullptr);

    shader_module_cache.AddName(sci->GetShaderStage(), sm_name, sm);

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

const ShaderModule *ShaderProgramManager::CreateShaderModule(const mtl::ShaderStageKey &key,
                                                             const ShaderCreateInfo *sci)
{
    if (!sci || key.stage != sci->GetShaderStage())
        return nullptr;

    return CreateShaderModule(key.ToString(), sci);
}

const ShaderModule *ShaderProgramManager::CreateShaderModuleFromSPV(const AnsiString &sm_name,
                                                                const VkShaderStageFlagBits stage,
                                                                const uint32_t *spv_data,
                                                                const size_t spv_size)
{
    VulkanDevice *device = GetDevice();
    if(!device)return(nullptr);
    if(sm_name.IsEmpty())return(nullptr);
    if(!spv_data||spv_size==0)return(nullptr);

    ShaderModule *sm = shader_module_cache.FindName(static_cast<ShaderStage>(stage), sm_name);
    if(sm)
        return sm;

    sm=device->CreateShaderModule(stage,spv_data,spv_size);

    if(!sm)
        return(nullptr);

    shader_module_cache.AddName(static_cast<ShaderStage>(stage), sm_name, sm);

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

const ShaderModule *ShaderProgramManager::CreateShaderModuleFromSPV(const mtl::ShaderStageKey &key,
                                                                     const uint32_t *spv_data,
                                                                     const size_t spv_size)
{
    return CreateShaderModuleFromSPV(key.ToString(),
                                     static_cast<VkShaderStageFlagBits>(key.stage),
                                     spv_data,
                                     spv_size);
}

PipelineLayoutData *ShaderProgramManager::CreateMaterialPipelineLayoutData(const AnsiString &mtl_name, const MaterialDescriptorManager *desc_manager)
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

MaterialParameters *ShaderProgramManager::CreateMaterialMP(const AnsiString &mtl_name, const MaterialDescriptorManager *desc_manager, const PipelineLayoutData *pld, const DescriptorSetType &desc_set_type)
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

void ShaderProgramManager::ApplyMaterialFinalizePlan(ShaderProgram *mtl, const AnsiString &mtl_name, const mtl::ShaderProgramBuildSpec &mci)
{
    if(!mtl)
        return;

    ShaderProgramFinalizePlan finalize_plan;
    BuildShaderProgramFinalizePlan(mtl->desc_manager, mci, finalize_plan);

    mtl->pipeline_layout_data = CreateMaterialPipelineLayoutData(mtl_name, mtl->desc_manager);

    for(const auto set_type : finalize_plan.mp_set_types)
    {
        mtl->mp_array[(int)set_type] = CreateMaterialMP(mtl_name, mtl->desc_manager, mtl->pipeline_layout_data, set_type);
    }

}

ShaderProgram *ShaderProgramManager::TryGetCachedMaterial(const AnsiString &name)
{
    return shader_program_cache.FindName(name);
}

ShaderProgram *ShaderProgramManager::TryGetCachedShaderProgram(const mtl::ShaderProgramKey &key)
{
    return shader_program_cache.Find(key);
}

bool ShaderProgramManager::RegisterShaderProgram(const mtl::ShaderProgramKey &key,
                                                 ShaderProgram *program)
{
    return shader_program_cache.Add(key, program);
}

bool ShaderProgramManager::ExecuteRuntimeMaterialBuildPipeline(ShaderProgram *mtl,
                                                          const AnsiString &mtl_name,
                                                          const mtl::ShaderProgramBuildSpec *mci,
                                                          const ShaderCreateInfoMap &sci_map)
{
    if(!mtl || !mci)
        return false;

    if(!BuildRuntimeShaderProgramState(mtl, mtl_name, mci, sci_map))
        return false;

    if(!BuildRuntimeDescriptorState(mtl, mtl_name, mci))
        return false;

    ApplyMaterialFinalizePlan(mtl, mtl_name, *mci);

    return true;
}

bool ShaderProgramManager::BuildRuntimeShaderProgramState(ShaderProgram *mtl,
                                                     const AnsiString &mtl_name,
                                                     const mtl::ShaderProgramBuildSpec *mci,
                                                     const ShaderCreateInfoMap &sci_map)
{
    if(!mtl || !mci)
        return false;

    if(!BuildShaderModulesFromCreateInfoMap(this,
                                            mtl_name,
                                            sci_map,
                                            mci,
                                            mtl->shader_maps))
    {
        return false;
    }

    CreateShaderStageList(mtl->shader_stage_list,mtl->shader_maps);

    const ShaderCreateInfoVertex *vert = mci->GetVertexShader();
    mtl->vertex_input = vert ? GetVertexInput(vert->GetInput()) : nullptr;

    return true;
}

bool ShaderProgramManager::BuildRuntimeDescriptorState(ShaderProgram *mtl,
                                                  const AnsiString &mtl_name,
                                                  const mtl::ShaderProgramBuildSpec *mci)
{
    if(!mtl || !mci)
        return false;

    std::vector<ShaderDescriptor> descriptors = CollectDescriptorsFromBuildSpec(mci);
    if(!descriptors.empty())
        mtl->desc_manager = new MaterialDescriptorManager(mtl_name, descriptors.data(), static_cast<uint>(descriptors.size()));
    else
        mtl->desc_manager = nullptr;

    return true;
}

ShaderProgram *ShaderProgramManager::AcquireShaderProgram(const AnsiString &mtl_name,const mtl::ShaderProgramBuildSpec *mci)
{
    HGL_CAPTURE_SCOPE();

    if(!mci)
        return(nullptr);

    ShaderProgramCreatePrecheckResult precheck_result;
    const ShaderProgramCreatePrecheckDecision precheck_decision = RunShaderProgramCreatePrecheck(
        mci,
        mtl_name,
        [&](const AnsiString &name)->ShaderProgram * { return TryGetCachedMaterial(name); },
        precheck_result);

    if(precheck_decision == ShaderProgramCreatePrecheckDecision::UseCached)
        return precheck_result.cached_material;

    if(precheck_decision != ShaderProgramCreatePrecheckDecision::Proceed)
    {
        GLogError("[ShaderProgramManager] shader program precheck rejected: name=%s decision=%u",
                  mtl_name.c_str(),
                  static_cast<uint32>(precheck_decision));
        return nullptr;
    }

    const ShaderCreateInfoMap &sci_map = *precheck_result.shader_map;

    AutoDelete<ShaderProgram> mtl=new ShaderProgram(mtl_name,mci);
    if(!ExecuteRuntimeMaterialBuildPipeline(mtl,
                                            mtl_name,
                                            mci,
                                            sci_map))
        return nullptr;

    Add(mtl);

    shader_program_cache.AddName(mtl_name,mtl);
    // ShaderProgram is a C++ object managed by ShaderProgramManager, not a Vulkan object
    // No need to track with ObjectTracker
    return mtl.Finish();
}

void ShaderProgramManager::ResetShaderGenProfiler()
{
    // Runtime path currently keeps ShaderGen debug APIs as no-op for compatibility.
}

ShaderGenProfilerSnapshot ShaderProgramManager::GetShaderGenProfilerSnapshot() const
{
    return {};
}

bool ShaderProgramManager::GetShaderGenLastValidationReport(ShaderGenValidationReport &out_report, std::string *out_material_name) const
{
    out_report = {};
    if (out_material_name)
        out_material_name->clear();
    return false;
}

std::vector<ShaderGenValidationReportRecord> ShaderProgramManager::GetShaderGenRecentValidationReports(const uint32_t max_count) const
{
    (void)max_count;
    return {};
}

std::map<std::string, uint32_t> ShaderProgramManager::GetShaderGenRecentValidationCategoryHistogram(const uint32_t max_count) const
{
    (void)max_count;
    return {};
}

bool ShaderProgramManager::BuildMaterialResourceLayout(const mtl::MaterialDefinitionBuildRequest &request,
                                                  mtl::MaterialResourceLayout &out_layout)
{
    mtl::MaterialDefinition bmi{};
    if (!ResolveMaterialDefinitionForRequest(
            request, &mtl::GetMaterialDefinitionFileRegistry(), bmi))
        return false;

    const auto *profile = GetPhysicalDeviceProfile();
    AutoDelete<mtl::ShaderProgramBuildSpec> mci = mtl::CreateMaterialFromDefinition(profile, bmi, request);
    if (!mci)
    {
        GLogError("[ShaderProgramManager] Material definition build failed: id=%s name=%s",
                  bmi.definition_id.c_str(),
                  bmi.definition_name.c_str());
        return false;
    }

    out_layout = mci->GetMaterialResourceLayout();
    return true;
}

ShaderProgram *ShaderProgramManager::AcquireShaderProgram(const mtl::MaterialDefinitionBuildRequest &request)
{
    // Ensure recipe defaults are filled in before lookup, in case the caller skipped normalization.
    // This call is idempotent; PrimitiveComponent-initiated paths will have already normalized.
    mtl::MaterialRecipe normalized_recipe = request.recipe;
    mtl::NormalizeRecipe(normalized_recipe);

    mtl::MaterialDefinitionBuildRequest normalized_request = request;
    normalized_request.recipe = normalized_recipe;

    mtl::MaterialDefinition bmi{};
    if (!ResolveMaterialDefinitionForRequest(
            normalized_request, &mtl::GetMaterialDefinitionFileRegistry(), bmi))
        return nullptr;

    // Compute hash key BEFORE generic shader compilation to avoid triggering
    // CompositorAssembler/GLSL compilation on every call when already cached.
    const std::string hash_std = bmi.definition_id + "?"
        + std::to_string(mtl::HashMaterialRecipe(normalized_request.recipe));
    const AnsiString hash_name = hash_std.c_str();

    if (ShaderProgram *cached = TryGetCachedMaterial(hash_name))
        return cached;

    const auto *profile = GetPhysicalDeviceProfile();
    AutoDelete<mtl::ShaderProgramBuildSpec> mci = mtl::CreateMaterialFromDefinition(profile, bmi, normalized_request);
    if(!mci)
    {
        GLogError("[ShaderProgramManager] Material definition build failed: id=%s name=%s",
                  bmi.definition_id.c_str(),
                  bmi.definition_name.c_str());
        return nullptr;
    }

    return this->AcquireShaderProgram(hash_name, mci);
}

}//namespace hgl::graph
