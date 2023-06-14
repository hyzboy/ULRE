#include<hgl/graph/VKShaderModule.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKShaderModuleMap.h>
#include<hgl/graph/VKShaderResource.h>
#include<hgl/graph/VKMaterialDescriptorManager.h>
#include<hgl/graph/VKVertexInput.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/VKRenderResource.h>
#include<hgl/io/ConstBufferReader.h>
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/ShaderDescriptorInfo.h>

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
            auto da=device->GetDeviceAttribute();

            if(da->debug_maker)
                da->debug_maker->SetShaderModule(*sm,sm_name);

            if(da->debug_utils)
                da->debug_utils->SetShaderModule(*sm,sm_name);
        }
    #endif//_DEBUG

    return sm;
}

Material *RenderResource::CreateMaterial(const mtl::MaterialCreateInfo *mci)
{
    if(!mci)
        return(nullptr);

    Material *mtl;

    const AnsiString mtl_name=mci->GetName();

    if(material_by_name.Get(mtl_name,mtl))
        return mtl;

    const ShaderCreateInfoMap &sci_map=mci->GetShaderMap();
    const uint sci_count=sci_map.GetCount();

    if(sci_count<2)
        return(nullptr);

    if(!mci->GetFS())
        return(nullptr);

    AutoDelete<MaterialData> data=new MaterialData(mtl_name);

    {
        const ShaderModule *sm;

        auto **sci=sci_map.GetDataList();

        for(uint i=0;i<sci_count;i++)
        {
            sm=CreateShaderModule(mtl_name,(*sci)->value);

            if(!sm)
                return(nullptr);

            data->shader_maps->Add(sm);

            if((*sci)->key==VK_SHADER_STAGE_VERTEX_BIT)
                data->vertex_input=new VertexInput((*sci)->value->sdm->GetShaderStageIO().input);

            ++sci;
        }

        CreateShaderStageList(data->shader_stage_list,data->shader_maps);
    }

    {
        const auto &mdi=mci->GetMDI();

        if(mdi.GetCount()>0)
            data->desc_manager=new MaterialDescriptorManager(mci->GetName(),mdi.Get());
    }

    data->pipeline_layout_data=device->CreatePipelineLayoutData(data->desc_manager);

    if(data->desc_manager)
    {
        ENUM_CLASS_FOR(DescriptorSetType,int,dst)
        {
            if(data->desc_manager->hasSet((DescriptorSetType)dst))
                data->mp_array[dst]=device->CreateMP(data->desc_manager,data->pipeline_layout_data,(DescriptorSetType)dst);
        }
    }

    data->mi_data_bytes =mci->GetMIDataBytes();
    data->mi_max_count  =mci->GetMIMaxCount();

    mtl=new Material(data);
    data.Discard();     //mtl已经接管了data的内容，这里不需要再释放

    Add(mtl);

    material_by_name.Add(mtl_name,mtl);
    return mtl;
}
VK_NAMESPACE_END
