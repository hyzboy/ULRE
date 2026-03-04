#include<hgl/graph/module/MaterialManager.h>
#include<hgl/vk/pipeline/VKPipelineLayoutData.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKObjectNameBuilder.h>
#include<hgl/vk/VKMaterial.h>
#include<hgl/vk/VKMaterialInstance.h>
#include<hgl/vk/VKMaterialParameters.h>
#include<hgl/vk/VKShaderModule.h>
#include<hgl/vk/VKShaderModuleMap.h>
#include<hgl/vk/VKMaterialDescriptorManager.h>
#include<hgl/vk/VKVertexInput.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/RendererShaderGenAdapter.h>
#include<hgl/graph/module/ShaderGenVertexInputAdapter.h>
#include<hgl/graph/module/ShaderGenVertexPolicyAdapter.h>
#include<hgl/graph/module/ShaderGenDescriptorLayoutAdapter.h>
#include<hgl/graph/module/ShaderGenDescriptorPolicyAdapter.h>
#include<hgl/graph/module/ShaderGenSPVModuleAdapter.h>
#include<hgl/graph/module/ShaderGenPathMode.h>
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/contract/ShaderGenRequestBuilder.h>
#include<hgl/shadergen/contract/ShaderGenResultBuilder.h>
#include<hgl/shadergen/ShaderDescriptorInfo.h>
#include<hgl/type/ActiveMemoryBlockManager.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/object/ObjectTracker.h>
#include<cstdint>
#include<cstdio>
#include<vector>
#include<string>

namespace hgl::graph{
namespace
{
    static void RecordStrictAbortReport(const AnsiString &mtl_name, const std::string &reason, const char *category)
    {
        RendererShaderGenAdapter::RecordExternalValidationError(
            mtl_name.c_str() ? mtl_name.c_str() : "<unnamed-material>",
            reason.c_str(),
            category);
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

    PipelineLayoutData *pld = device->CreatePipelineLayoutData(desc_manager);

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

Material *MaterialManager::CreateMaterial(const AnsiString &mtl_name,const mtl::MaterialCreateInfo *mci)
{
    HGL_CAPTURE_SCOPE();

    if(!mci)
        return(nullptr);

    const GraphicsContext *graphics_context = GetGraphicsContext();
    const ShaderGenPathMode path_mode = graphics_context ? graphics_context->GetShaderGenPathMode() : ShaderGenPathMode::MirrorValidate;
    const ShaderGenPathPolicy path_policy = graphics_context ? graphics_context->GetShaderGenPathPolicy() : MakeShaderGenPathPolicy(path_mode);
    const RendererShaderGenAdapter::DiffLogDetail diff_log_detail =
        path_policy.full_diff_log
        ? RendererShaderGenAdapter::DiffLogDetail::Full
        : RendererShaderGenAdapter::DiffLogDetail::SummaryOnly;

    mtl::contract::ShaderGenResult mirror_result;
    mtl::contract::ShaderGenRequest request_result;
    const mtl::contract::ShaderGenRequest *request_ptr = nullptr;
    const mtl::contract::ShaderGenResult *mirror_ptr = nullptr;

    if(path_policy.enable_mirror_validation && mtl::contract::BuildShaderGenRequestFromMaterialCreateInfo(*mci,request_result,mtl_name.c_str()))
    {
        request_ptr = &request_result;
    }

    if(path_policy.enable_mirror_validation && mtl::contract::BuildShaderGenResultFromMaterialCreateInfo(*mci,mirror_result))
    {
        mirror_ptr = &mirror_result;
    }
    else if(path_policy.enable_mirror_validation)
    {
        std::fprintf(stderr,
            "[RendererShaderGenAdapter] material=%s failed to prebuild mirror result (mode=%s)\n",
            mtl_name.c_str()?mtl_name.c_str():"<unnamed-material>",
            GetShaderGenPathModeName(path_mode));
    }

    if(path_policy.require_mirror_valid && !mirror_ptr)
    {
        RecordStrictAbortReport(mtl_name, "creation aborted: mirror-preferred requires valid mirror result", "StrictGate.Prebuild");
        std::fprintf(stderr,
            "[RendererShaderGenAdapter] material=%s creation aborted: mirror-preferred requires valid mirror result\n",
            mtl_name.c_str()?mtl_name.c_str():"<unnamed-material>");
        return nullptr;
    }

    return CreateMaterialWithContract(mtl_name,mci,request_ptr,mirror_ptr,path_policy.enable_mirror_validation,path_policy.require_mirror_valid,diff_log_detail);
}

Material *MaterialManager::CreateMaterialWithContract(const AnsiString &mtl_name,const mtl::MaterialCreateInfo *mci,const mtl::contract::ShaderGenRequest *request_result,const mtl::contract::ShaderGenResult *mirror_result,bool enable_mirror_validation,bool require_mirror_valid,const RendererShaderGenAdapter::DiffLogDetail diff_log_detail)
{
    if(!mci)
        return(nullptr);

    if(enable_mirror_validation)
    {
        RendererShaderGenAdapter adapter;
        const RendererShaderGenAdapter::ValidationReport consume_report=
            adapter.ValidateMaterialContractReadOnly(*mci,
                                                    request_result,
                                                    mirror_result,
                                                    mtl_name.c_str(),
                                                    diff_log_detail);

        if(!consume_report.overall_valid)
        {
            std::fprintf(stderr,
                "[RendererShaderGenAdapter] material=%s read-only validation failed (errors=%u, warnings=%u)\n",
                mtl_name.c_str()?mtl_name.c_str():"<unnamed-material>",
                consume_report.error_count,
                consume_report.warning_count);

            if(require_mirror_valid)
            {
                std::fprintf(stderr,
                    "[RendererShaderGenAdapter] material=%s creation aborted due to mirror-preferred strict mode\n",
                    mtl_name.c_str()?mtl_name.c_str():"<unnamed-material>");
                return nullptr;
            }
        }
    }

    {
        Material *mtl;

        if(material_by_name.Get(mtl_name,mtl))
            return mtl;
    }

    VulkanDevice *device = GetDevice();

    const ShaderCreateInfoMap &sci_map=mci->GetShaderMap();
    const uint sci_count=sci_map.GetCount();

    if(sci_count<2)
        return(nullptr);

    if(!mci->GetFS())
        return(nullptr);

    AutoDelete<Material> mtl=new Material(mtl_name,mci);
    const bool prefer_mirror_spv_build = mirror_result != nullptr;
    bool mirror_spv_build_used = false;

    {
        if(prefer_mirror_spv_build)
        {
            std::vector<const ShaderModule *> mirror_modules;
            std::string mirror_spv_fail_reason;

            const bool mirror_spv_build_ok = BuildShaderModulesFromContractSPV(
                *mirror_result,
                [&](VkShaderStageFlagBits stage, const uint32_t *spv_data, size_t spv_size)->const ShaderModule *
                {
                    return CreateShaderModuleFromSPV(mtl_name, stage, spv_data, spv_size);
                },
                mirror_modules,
                mirror_spv_fail_reason);

            if(mirror_spv_build_ok)
            {
                for(const auto *module : mirror_modules)
                    mtl->shader_maps->Add(module);

                mirror_spv_build_used = true;
            }
            else
            {
                if(require_mirror_valid)
                {
                    RecordStrictAbortReport(mtl_name, std::string("mirror-preferred build aborted: ") + mirror_spv_fail_reason, "StrictGate.Spv");
                    std::fprintf(stderr,
                        "[RendererShaderGenAdapter] material=%s mirror-preferred build aborted: %s\n",
                        mtl_name.c_str()?mtl_name.c_str():"<unnamed-material>",
                        mirror_spv_fail_reason.c_str());
                    return nullptr;
                }

                std::fprintf(stderr,
                    "[RendererShaderGenAdapter] material=%s mirror SPV build failed (%s), fallback to legacy path\n",
                    mtl_name.c_str()?mtl_name.c_str():"<unnamed-material>",
                    mirror_spv_fail_reason.c_str());
            }
        }

        if(!mirror_spv_build_used)
        {
            std::vector<const ShaderModule *> legacy_modules;
            std::string legacy_spv_fail_reason;

            if(!BuildShaderModulesFromLegacySCIMap(
                    sci_map,
                    [&](ShaderCreateInfo *sci_ptr)->const ShaderModule *
                    {
                        return CreateShaderModule(mtl_name, sci_ptr);
                    },
                    legacy_modules,
                    legacy_spv_fail_reason))
            {
                return nullptr;
            }

            for(const auto *module : legacy_modules)
                mtl->shader_maps->Add(module);
        }
    }

    CreateShaderStageList(mtl->shader_stage_list,mtl->shader_maps);

    {
        ShaderCreateInfoVertex *vert=mci->GetVS();

        VIAArray mirror_input;
        std::string reason;
        const ContractVertexInputDecision vertex_decision = BuildVertexInputByContractPolicy(
            vert,
            mirror_result,
            mirror_spv_build_used,
            require_mirror_valid,
            mirror_input,
            reason);

        if(vertex_decision == ContractVertexInputDecision::StrictAbort)
        {
            RecordStrictAbortReport(mtl_name, std::string("mirror-preferred build aborted: ") + reason, "StrictGate.Vertex");
            std::fprintf(stderr,
                "[RendererShaderGenAdapter] material=%s mirror-preferred build aborted: %s\n",
                mtl_name.c_str()?mtl_name.c_str():"<unnamed-material>",
                reason.c_str());
            return nullptr;
        }

        if(vertex_decision == ContractVertexInputDecision::UseLegacy && !reason.empty())
        {
            std::fprintf(stderr,
                "[RendererShaderGenAdapter] material=%s mirror vertex fallback (%s), use legacy vertex input\n",
                mtl_name.c_str()?mtl_name.c_str():"<unnamed-material>",
                reason.c_str());
        }

        if(vertex_decision == ContractVertexInputDecision::UseMirror)
            mtl->vertex_input = GetVertexInput(mirror_input);

        if(!mtl->vertex_input && vert)
            mtl->vertex_input=GetVertexInput(vert->GetInput());
    }

    {
        const auto &mdi=mci->GetMDI();

        std::vector<ShaderDescriptor> legacy_descriptors;
        if(mdi.GetCount()>0)
        {
            const auto &sds_array = mdi.Get();

            legacy_descriptors.reserve(mdi.GetCount());

            for(size_t i=0;i<DESCRIPTOR_SET_TYPE_COUNT;i++)
            {
                std::vector<ShaderDescriptor *> values;
                sds_array[i].descriptor_map.GetValueArray(values);

                for(auto *sd:values)
                    if(sd)
                        legacy_descriptors.emplace_back(*sd);
            }
        }

        std::vector<ShaderDescriptor> descriptors;
        ContractDescriptorFallbackPhase descriptor_phase = ContractDescriptorFallbackPhase::None;
        std::string descriptor_reason;
        const ContractDescriptorDecision descriptor_decision = BuildDescriptorsByContractPolicy(
            legacy_descriptors,
            mirror_result,
            mirror_spv_build_used,
            require_mirror_valid,
            descriptors,
            descriptor_phase,
            descriptor_reason);

        if(descriptor_decision == ContractDescriptorDecision::StrictAbort)
        {
            RecordStrictAbortReport(mtl_name, std::string("mirror-preferred build aborted: ") + descriptor_reason, "StrictGate.Descriptor");
            std::fprintf(stderr,
                "[RendererShaderGenAdapter] material=%s mirror-preferred build aborted: %s\n",
                mtl_name.c_str()?mtl_name.c_str():"<unnamed-material>",
                descriptor_reason.c_str());
            return nullptr;
        }

        if(descriptor_decision == ContractDescriptorDecision::UseLegacy && !descriptor_reason.empty())
        {
            const char *fallback_phase_text = (descriptor_phase == ContractDescriptorFallbackPhase::LayoutMismatch)
                                              ? "layout mismatch"
                                              : "layout build failed";

            std::fprintf(stderr,
                "[RendererShaderGenAdapter] material=%s mirror descriptor %s (%s), fallback to legacy descriptor layout\n",
                mtl_name.c_str()?mtl_name.c_str():"<unnamed-material>",
                fallback_phase_text,
                descriptor_reason.c_str());
        }

        if(!descriptors.empty())
            mtl->desc_manager=new MaterialDescriptorManager(mtl_name,descriptors.data(),static_cast<uint>(descriptors.size()));
    }

    mtl->pipeline_layout_data=CreateMaterialPipelineLayoutData(mtl_name, mtl->desc_manager);

    if(mtl->desc_manager)
    {
        ENUM_CLASS_FOR(DescriptorSetType,int,dst)
        {
            if(mtl->desc_manager->hasSet((DescriptorSetType)dst))
            {
                mtl->mp_array[dst]=CreateMaterialMP(mtl_name, mtl->desc_manager, mtl->pipeline_layout_data, (DescriptorSetType)dst);
            }
        }
    }

    mtl->mi_data_bytes =mci->GetMIDataBytes();
    mtl->mi_max_count  =mci->GetMIMaxCount();

    if(mtl->mi_data_bytes>0)
    {
        mtl->mi_data_manager=new ActiveMemoryBlockManager(mtl->mi_data_bytes);
    }

    Add(mtl);

    material_by_name.Add(mtl_name,mtl);
    // Material is a C++ object managed by MaterialManager, not a Vulkan object
    // No need to track with ObjectTracker
    return mtl.Finish();
}

Material *MaterialManager::CreateMaterial(const mtl::MaterialPreset mtl_id,mtl::Material2DCreateConfig *cfg)
{
    HGL_CAPTURE_SCOPE();

    if(!cfg)
        return(nullptr);

    AutoDelete<mtl::MaterialCreateInfo> mci=mtl::CreateMaterialCreateInfo(GetDevAttr(),mtl_id,cfg);

    if(!mci)
        return(nullptr);

    AnsiString hash_name=mtl::GetInlineMaterialName(mtl_id);
    hash_name+="?";
    hash_name+=cfg->ToHashStdString().c_str();

    return this->CreateMaterial(hash_name,mci);
}

Material *MaterialManager::CreateMaterial(const mtl::MaterialPreset mtl_id,mtl::Material3DCreateConfig *cfg)
{
    HGL_CAPTURE_SCOPE();

    if(!cfg)
        return(nullptr);

    AutoDelete<mtl::MaterialCreateInfo> mci=mtl::CreateMaterialCreateInfo(GetDevAttr(),mtl_id,cfg);

    if(!mci)
        return(nullptr);

    AnsiString hash_name=mtl::GetInlineMaterialName(mtl_id);
    hash_name+="?";
    hash_name+=cfg->ToHashStdString().c_str();

    return this->CreateMaterial(hash_name,mci);
}

MaterialInstance *MaterialManager::CreateMaterialInstance(Material *mtl)
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

MaterialInstance *MaterialManager::CreateMaterialInstance(Material *mtl,const VIL *vil)
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

MaterialInstance *MaterialManager::CreateMaterialInstance(Material *mtl,const VILConfig *vil_cfg)
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

MaterialInstance *MaterialManager::CreateMaterialInstance(Material *mtl,const VIL *vil,const void *mi_data,const uint32 mi_bytes)
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
        mi->WriteMIData(mi_data,mi_bytes);

    return mi;
}

MaterialInstance *MaterialManager::CreateMaterialInstance(Material *mtl,const VILConfig *vil_cfg,const void *mi_data,const uint32 mi_bytes)
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
        mi->WriteMIData(mi_data,mi_bytes);

    return mi;
}

MaterialInstance *MaterialManager::CreateMaterialInstance(const mtl::MaterialPreset mtl_id,mtl::Material2DCreateConfig *mcc,const VILConfig *vil_cfg,const void *data,const uint32 data_size)
{
    HGL_CAPTURE_SCOPE();

    Material *mtl=this->CreateMaterial(mtl_id,mcc);

    if(!mtl)
        return(nullptr);

    return CreateMaterialInstance(mtl,vil_cfg,data,data_size);
}

MaterialInstance *MaterialManager::CreateMaterialInstance(const mtl::MaterialPreset mtl_id,mtl::Material3DCreateConfig *mcc,const VILConfig *vil_cfg,const void *data,const uint32 data_size)
{
    HGL_CAPTURE_SCOPE();

    Material *mtl=this->CreateMaterial(mtl_id,mcc);

    if(!mtl)
        return(nullptr);

    return CreateMaterialInstance(mtl,vil_cfg,data,data_size);
}

}//namespace hgl::graph
