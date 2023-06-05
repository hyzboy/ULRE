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
const ShaderModule *RenderResource::CreateShaderModule(const AnsiString &sm_name,VkShaderStageFlagBits shader_stage,const uint32_t *spv_data,const size_t spv_size)
{
    if(!device)return(nullptr);
    if(sm_name.IsEmpty())return(nullptr);
    if(!spv_data)return(nullptr);
    if(spv_size<4)return(nullptr);

    ShaderModule *sm;

    if(shader_module_by_name.Get(sm_name,sm))
        return sm;

    sm=device->CreateShaderModule(shader_stage,spv_data,spv_size);

    shader_module_by_name.Add(sm_name,sm);

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

    const uint count=GetShaderCountByBits(mci->GetShaderStage());
    const ShaderModule *sm;

    ShaderModuleMap *smm=new ShaderModuleMap;
    VertexInput *vertex_input=nullptr;

    const ShaderCreateInfoVertex *vert=mci->GetVS();

    if(vert)
    {
        sm=CreateShaderModule(  mtl_name+U8_TEXT("?Vertex"),
                                VK_SHADER_STAGE_VERTEX_BIT,
                                vert->GetSPVData(),vert->GetSPVSize());

        if(!sm)
            return(false);

        if(smm->Add(sm))
            vertex_input=new VertexInput(vert->sdm->GetShaderStageIO().input);
    }

    const ShaderCreateInfoGeometry *geom=mci->GetGS();

    if(geom)
    {
        sm=CreateShaderModule(  mtl_name+U8_TEXT("?Geometry"),
                                VK_SHADER_STAGE_GEOMETRY_BIT,
                                geom->GetSPVData(),geom->GetSPVSize());

        smm->Add(sm);
    }

    const ShaderCreateInfoFragment *frag=mci->GetFS();

    if(frag)
    {
        sm=CreateShaderModule(  mtl_name+U8_TEXT("?Fragment"),
                                VK_SHADER_STAGE_FRAGMENT_BIT,
                                frag->GetSPVData(),frag->GetSPVSize());

        smm->Add(sm);
    }

    MaterialDescriptorManager *mdm=nullptr;

    {
        const auto &mdi=mci->GetMDI();

        if(mdi.GetCount()>0)
            mdm=new MaterialDescriptorManager(mci->GetName(),mdi.Get());
    }

    mtl=device->CreateMaterial(mci->GetName(),smm,mdm,vertex_input);

    if(!mtl)
    {
        delete mdm;
        delete smm;
    }
    else
    {
        Add(mtl);
    }

    material_by_name.Add(mtl_name,mtl);
    return mtl;
}
VK_NAMESPACE_END
