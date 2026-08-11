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
#include<hgl/shadergen/ShaderBuildContext.h>
#include<hgl/shadergen/MaterialShaderCompiler.h>
#include<hgl/shadergen/ShaderArtifactStore.h>
#include<hgl/shadergen/ShaderCreateInfoVertex.h>
#include<hgl/mtl/MaterialDefinitionRegistry.h>
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
        (void)file_registry;
        const std::string &mtl_def_id = request.recipe.mtl_def_id;

        if (mtl::TryGetMaterialDefinitionByID(mtl_def_id, out_bmi))
            return true;

        return mtl::TryGetMaterialDefinitionByID(
            mtl::GetFallbackMaterialDefinitionID(), out_bmi);
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
                                             const shadergen::ShaderCreateInfoMap &sci_map,
                                             const shadergen::ShaderBuildContext *build_spec,
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
            shadergen::ShaderArtifactStore *artifact_store =
                build_spec ? build_spec->GetArtifactStore() : nullptr;
            const shadergen::ShaderStageKey *cache_key = nullptr;
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

    std::vector<ShaderDescriptor> CollectDescriptorsFromBuildSpec(const shadergen::ShaderBuildContext *mci)
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

const ShaderModule *ShaderProgramManager::CreateShaderModule(const AnsiString &sm_name,const shadergen::ShaderCreateInfo *sci)
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

const ShaderModule *ShaderProgramManager::CreateShaderModule(const shadergen::ShaderStageKey &key,
                                                             const shadergen::ShaderCreateInfo *sci)
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

const ShaderModule *ShaderProgramManager::CreateShaderModuleFromSPV(const shadergen::ShaderStageKey &key,
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

void ShaderProgramManager::ApplyMaterialFinalizePlan(ShaderProgram *mtl, const AnsiString &mtl_name, const shadergen::ShaderBuildContext &mci)
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

ShaderProgram *ShaderProgramManager::TryGetCachedShaderProgram(
    const shadergen::ShaderProgramKey &key)
{
    return shader_program_cache.Find(key);
}

bool ShaderProgramManager::ExecuteRuntimeMaterialBuildPipeline(ShaderProgram *mtl,
                                                          const AnsiString &mtl_name,
                                                          const shadergen::ShaderBuildContext *mci,
                                                          const shadergen::ShaderCreateInfoMap &sci_map)
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
                                                     const shadergen::ShaderBuildContext *mci,
                                                     const shadergen::ShaderCreateInfoMap &sci_map)
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

    const shadergen::ShaderCreateInfoVertex *vert = mci->GetVertexShader();
    mtl->vertex_input = vert ? GetVertexInput(vert->GetInput()) : nullptr;

    return true;
}

bool ShaderProgramManager::BuildRuntimeDescriptorState(ShaderProgram *mtl,
                                                  const AnsiString &mtl_name,
                                                  const shadergen::ShaderBuildContext *mci)
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

ShaderProgram *ShaderProgramManager::AcquireShaderProgram(
    const shadergen::ShaderProgramKey &program_key,
    const shadergen::ShaderBuildContext *mci)
{
    HGL_CAPTURE_SCOPE();

    if (!mci
     || !mci->HasProgramLink()
     || !(mci->GetProgramLink().BuildKey() == program_key))
        return(nullptr);

    const AnsiString mtl_name = program_key.ToString();
    ShaderProgramCreatePrecheckResult precheck_result;
    const ShaderProgramCreatePrecheckDecision precheck_decision = RunShaderProgramCreatePrecheck(
        mci,
        mtl_name,
        precheck_result);

    if(precheck_decision != ShaderProgramCreatePrecheckDecision::Proceed)
    {
        GLogError("[ShaderProgramManager] shader program precheck rejected: name=%s decision=%u",
                  mtl_name.c_str(),
                  static_cast<uint32>(precheck_decision));
        return nullptr;
    }

    const shadergen::ShaderCreateInfoMap &sci_map = *precheck_result.shader_map;

    AutoDelete<ShaderProgram> mtl=new ShaderProgram(mtl_name,mci);
    if(!ExecuteRuntimeMaterialBuildPipeline(mtl,
                                            mtl_name,
                                            mci,
                                            sci_map))
        return nullptr;

    Add(mtl);

    shader_program_cache.Add(program_key, mtl);
    // ShaderProgram is a C++ object managed by ShaderProgramManager, not a Vulkan object
    // No need to track with ObjectTracker
    return mtl.Finish();
}

bool ShaderProgramManager::BuildShaderResourceSchema(const mtl::MaterialDefinitionBuildRequest &request,
                                                  mtl::ShaderResourceSchema &out_layout)
{
    mtl::MaterialDefinition bmi{};
    if (!ResolveMaterialDefinitionForRequest(
            request, &mtl::GetMaterialDefinitionFileRegistry(), bmi))
        return false;

    const auto *profile = GetPhysicalDeviceProfile();
    AutoDelete<shadergen::ShaderBuildContext> mci = mtl::CreateMaterialFromDefinition(profile, bmi, request);
    if (!mci)
    {
        GLogError("[ShaderProgramManager] Material definition build failed: id=%s name=%s",
                  bmi.definition_id.c_str(),
                  bmi.definition_name.c_str());
        return false;
    }

    out_layout = mci->GetShaderResourceSchema();
    return true;
}

ShaderProgram *ShaderProgramManager::AcquireShaderProgram(
    const mtl::MaterialDefinitionBuildRequest &request)
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

    const auto *profile = GetPhysicalDeviceProfile();
    normalized_request.generate_only = true;
    AutoDelete<shadergen::ShaderBuildContext> mci =
        mtl::CreateMaterialFromDefinition(
            profile, bmi, normalized_request);
    if(!mci)
    {
        GLogError("[ShaderProgramManager] Material definition build failed: id=%s name=%s",
                  bmi.definition_id.c_str(),
                  bmi.definition_name.c_str());
        return nullptr;
    }

    if (!mci->HasProgramLink())
    {
        GLogError(
            "[ShaderProgramManager] Material build produced incomplete program identity: id=%s",
            bmi.definition_id.c_str());
        return nullptr;
    }
    const shadergen::ShaderProgramKey program_key =
        mci->GetProgramLink().BuildKey();
    if (ShaderProgram *cached = TryGetCachedShaderProgram(program_key))
        return cached;

    if (!shadergen::FinalizeShaderProgramBuildSpec(mci))
    {
        GLogError(
            "[ShaderProgramManager] Material build finalization failed: id=%s",
            bmi.definition_id.c_str());
        return nullptr;
    }

    return this->AcquireShaderProgram(program_key, mci);
}

}//namespace hgl::graph
