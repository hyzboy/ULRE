#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/VKShaderModule.h>
#include<hgl/graph/VKShaderModuleMap.h>
#include<hgl/graph/VKMaterialDescriptorManager.h>
#include<hgl/graph/VKVertexInput.h>
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/ShaderDescriptorInfo.h>
#include<hgl/type/ActiveMemoryBlockManager.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>

#ifdef _DEBUG
#include"../Vulkan/VKPipelineLayoutData.h"
#endif//_DEBUG

VK_NAMESPACE_BEGIN
namespace
{
    void CreateShaderStageList(ArrayList<VkPipelineShaderStageCreateInfo> &shader_stage_list,ShaderModuleMap *shader_maps)
    {
        const ShaderModule *sm;

        const int shader_count=shader_maps->GetCount();
        shader_stage_list.SetCount(shader_count);
    
        VkPipelineShaderStageCreateInfo *p=shader_stage_list.GetData();

        auto **itp=shader_maps->GetDataList();
        for(int i=0;i<shader_count;i++)
        {
            sm=(*itp)->value;
            hgl_cpy(p,sm->GetCreateInfo(),1);

            ++p;
            ++itp;
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
                du->SetShaderModule(*sm,"Shader:"+sm_name+AnsiString(":")+GetShaderStageName((VkShaderStageFlagBits)sci->GetShaderStage()));
        }
    #endif//_DEBUG

    return sm;
}

Material *MaterialManager::CreateMaterial(const AnsiString &mtl_name,const mtl::MaterialCreateInfo *mci)
{
    if(!mci)
        return(nullptr);

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

        auto **sci=sci_map.GetDataList();

        for(uint i=0;i<sci_count;i++)
        {
            sm=CreateShaderModule(mtl_name,(*sci)->value);

            if(!sm)
                return(nullptr);

            mtl->shader_maps->Add(sm);

            ++sci;
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

    mtl->pipeline_layout_data=device->CreatePipelineLayoutData(mtl->desc_manager);

    #ifdef _DEBUG
        DebugUtils *du=device->GetDebugUtils();

        if(du)
            du->SetPipelineLayout(mtl->GetPipelineLayout(),"PipelineLayout:"+mtl->GetName());
    #endif//_DEBUG

    if(mtl->desc_manager)
    {
        ENUM_CLASS_FOR(DescriptorSetType,int,dst)
        {
            if(mtl->desc_manager->hasSet((DescriptorSetType)dst))
            {
                mtl->mp_array[dst]=device->CreateMP(mtl->desc_manager,mtl->pipeline_layout_data,(DescriptorSetType)dst);

            #ifdef _DEBUG
                AnsiString debug_name=mtl->GetName()+AnsiString(":")+GetDescriptorSetTypeName((DescriptorSetType)dst);
        
                if(du)
                {
                    du->SetDescriptorSet(mtl->mp_array[dst]->GetVkDescriptorSet(),"DescSet:"+debug_name);

                    du->SetDescriptorSetLayout(mtl->pipeline_layout_data->layouts[dst],"DescSetLayout:"+debug_name);
                }
            #endif//_DEBUG
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
    return mtl.Finish();
}

namespace mtl
{
    MaterialCreateInfo *LoadMaterialFromFile(const VulkanDevAttr *dev_attr,const AnsiString &, Material2DCreateConfig *);
    MaterialCreateInfo *LoadMaterialFromFile(const VulkanDevAttr *dev_attr,const AnsiString &, Material3DCreateConfig *);
}

Material *MaterialManager::LoadMaterial(const AnsiString &mtl_name,mtl::Material2DCreateConfig *cfg)
{
    AutoDelete<mtl::MaterialCreateInfo> mci=mtl::LoadMaterialFromFile(GetDevAttr(),mtl_name,cfg);

    //这里直接用这个mtl_name有些不太对，因为同一个材质，也有可能因为不同的cfg会有不同的版本，所以这里不能直接使用mtl_name.目前只是做一个暂时方案

    AnsiString hash_name=mtl_name+"?"+cfg->ToHashString();

    return this->CreateMaterial(hash_name,mci);
}

Material *MaterialManager::LoadMaterial(const AnsiString &mtl_name,mtl::Material3DCreateConfig *cfg)
{
    AutoDelete<mtl::MaterialCreateInfo> mci=mtl::LoadMaterialFromFile(GetDevAttr(),mtl_name,cfg);

    //这里直接用这个mtl_name有些不太对，因为同一个材质，也有可能因为不同的cfg会有不同的版本，所以这里不能直接使用mtl_name.目前只是做一个暂时方案

    AnsiString hash_name=mtl_name+"?"+cfg->ToHashString();

    return this->CreateMaterial(hash_name,mci);
}


MaterialInstance *MaterialManager::CreateMaterialInstance(Material *mtl)
{
    if(!mtl)return(nullptr);

    MaterialInstance *mi=mtl->CreateMI();

    if(mi)
        Add(mi);

    return mi;
}

MaterialInstance *MaterialManager::CreateMaterialInstance(Material *mtl,const VIL *vil)
{
    if(!mtl)return(nullptr);

    MaterialInstance *mi=mtl->CreateMI(vil);

    if(mi)
        Add(mi);

    return mi;
}

MaterialInstance *MaterialManager::CreateMaterialInstance(Material *mtl,const VILConfig *vil_cfg)
{
    if(!mtl)return(nullptr);

    MaterialInstance *mi=mtl->CreateMI(vil_cfg);

    if(mi)
        Add(mi);

    return mi;
}

MaterialInstance *MaterialManager::CreateMaterialInstance(Material *mtl,const VIL *vil,const void *mi_data,const size_t mi_bytes)
{
    if(!mtl)return(nullptr);

    MaterialInstance *mi=mtl->CreateMI(vil);

    if(!mi)
        return nullptr;

    Add(mi);

    if(mi_data&&mi_bytes>0)
        mi->WriteMIData(mi_data,mi_bytes);

    return mi;
}

MaterialInstance *MaterialManager::CreateMaterialInstance(Material *mtl,const VILConfig *vil_cfg,const void *mi_data,const size_t mi_bytes)
{
    if(!mtl)return(nullptr);

    MaterialInstance *mi=mtl->CreateMI(vil_cfg);

    if(!mi)
        return nullptr;

    Add(mi);

    if(mi_data&&mi_bytes>0)
        mi->WriteMIData(mi_data,mi_bytes);

    return mi;
}

MaterialInstance *MaterialManager::CreateMaterialInstance(const AnsiString &mtl_name,const mtl::MaterialCreateInfo *mci,const VILConfig *vil_cfg)
{
    Material *mtl=this->CreateMaterial(mtl_name,mci);

    if(!mtl)
        return(nullptr);

    return CreateMaterialInstance(mtl,vil_cfg);
}

MaterialInstance *MaterialManager::CreateMaterialInstance(const AnsiString &mtl_name, const mtl::MaterialCreateInfo *mci, const VILConfig *vil_cfg,const void *data,const size_t data_size)
{
    Material *mtl=this->CreateMaterial(mtl_name,mci);

    if(!mtl)
        return(nullptr);

    return CreateMaterialInstance(mtl,vil_cfg,data,data_size);
}

MaterialInstance *MaterialManager::CreateMaterialInstance(const AnsiString &mtl_name,mtl::Material2DCreateConfig *mcc,const VILConfig *vil_cfg,const void *data,const size_t data_size)
{
    mtl::MaterialCreateInfo *mci=CreateMaterialCreateInfo(GetDevAttr(),mtl_name,mcc);

    if(!mci)
        return(nullptr);

    return CreateMaterialInstance(mtl_name,mci,vil_cfg,data,data_size);
}

MaterialInstance *MaterialManager::CreateMaterialInstance(const AnsiString &mtl_name,mtl::Material3DCreateConfig *mcc,const VILConfig *vil_cfg,const void *data,const size_t data_size)
{
    mtl::MaterialCreateInfo *mci=CreateMaterialCreateInfo(GetDevAttr(),mtl_name,mcc);

    if(!mci)
        return(nullptr);

    return CreateMaterialInstance(mtl_name,mci,vil_cfg,data,data_size);
}

VK_NAMESPACE_END
