#include<hgl/graph/VKShaderModule.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKShaderModuleMap.h>
#include<hgl/graph/VKMaterialDescriptorManager.h>
#include<hgl/graph/VKVertexInput.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/VKRenderResource.h>
#include<hgl/io/ConstBufferReader.h>
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/ShaderDescriptorInfo.h>
#include<hgl/type/ActiveMemoryBlockManager.h>

#ifdef _DEBUG
#include"VKPipelineLayoutData.h"
#endif//_DEBUG

VK_NAMESPACE_BEGIN
namespace
{
    void CreateShaderStageList(List<VkPipelineShaderStageCreateInfo> &shader_stage_list,ShaderModuleMap *shader_maps)
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

const ShaderModule *RenderResource::CreateShaderModule(const AnsiString &sm_name,const ShaderCreateInfo *sci)
{
    if(!device)return(nullptr);
    if(sm_name.IsEmpty())return(nullptr);

    const int bit_offset=GetBitOffset((uint32_t)sci->GetShaderStage());

    if(bit_offset<0||bit_offset>VK_SHADER_STAGE_TYPE_COUNT)return(nullptr);

    ShaderModule *sm;

    ShaderModuleMapByName &sm_map=shader_module_by_name[bit_offset];

    if(sm_map.Get(sm_name,sm))
        return sm;

    sm=device->CreateShaderModule(sci->GetShaderStage(),sci->GetSPVData(),sci->GetSPVSize());

    if(!sm)
        return(nullptr);

    sm_map.Add(sm_name,sm);

    #ifdef _DEBUG
        {
            DebugUtils *du=device->GetDebugUtils();

            if(du)
                du->SetShaderModule(*sm,"Shader:"+sm_name+AnsiString(":")+GetShaderStageName(sci->GetShaderStage()));
        }
    #endif//_DEBUG

    return sm;
}

Material *RenderResource::CreateMaterial(const mtl::MaterialCreateInfo *mci)
{
    if(!mci)
        return(nullptr);

    const AnsiString &mtl_name=mci->GetName();

    {
        Material *mtl;

        if(material_by_name.Get(mtl_name,mtl))
            return mtl;
    }

    const ShaderCreateInfoMap &sci_map=mci->GetShaderMap();
    const uint sci_count=sci_map.GetCount();

    if(sci_count<2)
        return(nullptr);

    if(!mci->GetFS())
        return(nullptr);

    AutoDelete<Material> mtl=new Material(mtl_name);

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
            mtl->desc_manager=new MaterialDescriptorManager(mci->GetName(),mdi.Get());
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

    static_descriptor.Bind(mtl);
    global_descriptor.Bind(mtl);

    material_by_name.Add(mtl_name,mtl);
    return mtl.Finish();
}

namespace mtl
{
    MaterialCreateInfo *LoadMaterialFromFile(const AnsiString &, Material2DCreateConfig *);
    MaterialCreateInfo *LoadMaterialFromFile(const AnsiString &, Material3DCreateConfig *);
}

Material *RenderResource::LoadMaterial(const AnsiString &mtl_name,mtl::Material2DCreateConfig *cfg)
{
    AutoDelete<mtl::MaterialCreateInfo> mci=mtl::LoadMaterialFromFile(mtl_name,cfg);

    return this->CreateMaterial(mci);
}

Material *RenderResource::LoadMaterial(const AnsiString &mtl_name,mtl::Material3DCreateConfig *cfg)
{
    AutoDelete<mtl::MaterialCreateInfo> mci=mtl::LoadMaterialFromFile(mtl_name,cfg);

    return this->CreateMaterial(mci);
}
VK_NAMESPACE_END
