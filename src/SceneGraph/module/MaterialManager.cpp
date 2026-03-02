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
#include<hgl/graph/module/RendererShaderGenAdapter.h>
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/contract/ShaderGenResultBuilder.h>
#include<hgl/shadergen/ShaderDescriptorInfo.h>
#include<hgl/type/ActiveMemoryBlockManager.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/object/ObjectTracker.h>
#include<cstdint>
#include<cstdio>

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

    mtl::contract::ShaderGenResult mirror_result;
    const mtl::contract::ShaderGenResult *mirror_ptr=nullptr;

    if(mtl::contract::BuildShaderGenResultFromMaterialCreateInfo(*mci,mirror_result))
    {
        mirror_ptr=&mirror_result;
    }
    else
    {
        std::fprintf(stderr,
            "[RendererShaderGenAdapter] material=%s failed to prebuild mirror result, fallback to legacy-only validation\n",
            mtl_name.c_str()?mtl_name.c_str():"<unnamed-material>");
    }

    return CreateMaterialWithContract(mtl_name,mci,mirror_ptr);
}

Material *MaterialManager::CreateMaterialWithContract(const AnsiString &mtl_name,const mtl::MaterialCreateInfo *mci,const mtl::contract::ShaderGenResult *mirror_result)
{
    if(!mci)
        return(nullptr);

    {
        RendererShaderGenAdapter adapter;

        bool consume_ok=false;
        if(mirror_result)
            consume_ok=adapter.ConsumeResultReadOnly(*mirror_result,mtl_name.c_str());
        else
            consume_ok=adapter.ConsumeMaterialReadOnly(*mci,mtl_name.c_str());

        if(!consume_ok)
        {
            std::fprintf(stderr,
                "[RendererShaderGenAdapter] material=%s read-only consume detected structural issues\n",
                mtl_name.c_str()?mtl_name.c_str():"<unnamed-material>");
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

    {
        const ShaderModule *sm;

        for(auto [stage, sci_ptr] : sci_map)
        {
            sm=CreateShaderModule(mtl_name, sci_ptr);

            if(!sm)
                return(nullptr);

            mtl->shader_maps->Add(sm);
        }
    }

    CreateShaderStageList(mtl->shader_stage_list,mtl->shader_maps);

    {
        ShaderCreateInfoVertex *vert=mci->GetVS();

        if(vert)
            mtl->vertex_input=GetVertexInput(vert->GetInput());
    }

    {
        const auto &mdi=mci->GetMDI();

        if(mdi.GetCount()>0)
            mtl->desc_manager=new MaterialDescriptorManager(mtl_name,mdi.Get());
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

Material *MaterialManager::CreateMaterial(const mtl::InlineMaterial mtl_id,mtl::Material2DCreateConfig *cfg)
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

Material *MaterialManager::CreateMaterial(const mtl::InlineMaterial mtl_id,mtl::Material3DCreateConfig *cfg)
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

MaterialInstance *MaterialManager::CreateMaterialInstance(const mtl::InlineMaterial mtl_id,mtl::Material2DCreateConfig *mcc,const VILConfig *vil_cfg,const void *data,const uint32 data_size)
{
    HGL_CAPTURE_SCOPE();

    Material *mtl=this->CreateMaterial(mtl_id,mcc);

    if(!mtl)
        return(nullptr);

    return CreateMaterialInstance(mtl,vil_cfg,data,data_size);
}

MaterialInstance *MaterialManager::CreateMaterialInstance(const mtl::InlineMaterial mtl_id,mtl::Material3DCreateConfig *mcc,const VILConfig *vil_cfg,const void *data,const uint32 data_size)
{
    HGL_CAPTURE_SCOPE();

    Material *mtl=this->CreateMaterial(mtl_id,mcc);

    if(!mtl)
        return(nullptr);

    return CreateMaterialInstance(mtl,vil_cfg,data,data_size);
}

}//namespace hgl::graph
