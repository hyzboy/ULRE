#include<hgl/graph/vulkan/VKShaderModuleManage.h>
#include<hgl/graph/vulkan/VKShaderModule.h>
#include<hgl/graph/vulkan/VKMaterial.h>
#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/graph/vulkan/VKShaderParse.h>
#include<hgl/filesystem/FileSystem.h>

VK_NAMESPACE_BEGIN
Material *CreateMaterial(Device *dev,ShaderModuleMap *shader_maps);

ShaderModuleManage::~ShaderModuleManage()
{
    const int count=shader_list.GetCount();

    if(count>0)
    {
        auto **p=shader_list.GetDataList();

        for(int i=0;i<count;i++)
        {
            delete (*p)->right;

            ++p;
        }
    }
}

const ShaderModule *ShaderModuleManage::CreateShader(const VkShaderStageFlagBits shader_stage_bit,const void *spv_data,const uint32_t spv_size)
{
    VkPipelineShaderStageCreateInfo *shader_stage=new VkPipelineShaderStageCreateInfo;
    shader_stage->sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage->pNext=nullptr;
    shader_stage->pSpecializationInfo=nullptr;
    shader_stage->flags=0;
    shader_stage->stage=shader_stage_bit;
    shader_stage->pName="main";

    VkShaderModuleCreateInfo moduleCreateInfo;
    moduleCreateInfo.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleCreateInfo.pNext=nullptr;
    moduleCreateInfo.flags=0;
    moduleCreateInfo.codeSize=spv_size;
    moduleCreateInfo.pCode=(const uint32_t *)spv_data;

    if(vkCreateShaderModule(*device,&moduleCreateInfo,nullptr,&(shader_stage->module))!=VK_SUCCESS)
        return(nullptr);

    ShaderModule *sm;
    ShaderParse *parse=new ShaderParse(spv_data,spv_size);

    if(shader_stage_bit==VK_SHADER_STAGE_VERTEX_BIT)
        sm=new VertexShaderModule(shader_count,shader_stage,parse);
    else
        sm=new ShaderModule(shader_count,shader_stage,parse);

    delete parse;
    shader_list.Add(shader_count,sm);

    ++shader_count;

    return sm;
}

const ShaderModule *ShaderModuleManage::CreateShader(const VkShaderStageFlagBits shader_stage_bit,const OSString &filename)
{
    uint32_t spv_size;
    void *spv_data;
    spv_size=hgl::filesystem::LoadFileToMemory(filename,&spv_data);

    if(!spv_data)
        return(nullptr);

    const ShaderModule *sm=CreateShader(shader_stage_bit,spv_data,spv_size);

    delete[] spv_data;

    return sm;
}

const ShaderModule *ShaderModuleManage::GetShader(int id)
{
    ShaderModule *sm;

    if(!shader_list.Get(id,sm))
        return nullptr;

    sm->IncRef();
    return sm;
}

bool ShaderModuleManage::ReleaseShader(const ShaderModule *const_sm)
{
    if(!const_sm)
        return(false);

    ShaderModule *sm;

    if(!shader_list.Get(const_sm->GetID(),sm))
        return(false);

    if(sm!=const_sm)
        return(false);

    sm->DecRef();
    return(true);
}

Material *ShaderModuleManage::CreateMaterial(const VertexShaderModule *vertex_shader_module,const ShaderModule *fragment_shader_module)const
{
    if(!vertex_shader_module||!fragment_shader_module)
        return(nullptr);

    if(vertex_shader_module->GetStage()!=VK_SHADER_STAGE_VERTEX_BIT)return(nullptr);
    if(fragment_shader_module->GetStage()!=VK_SHADER_STAGE_FRAGMENT_BIT)return(nullptr);

    ShaderModuleMap *smm=new ShaderModuleMap;

    smm->Add(VK_SHADER_STAGE_VERTEX_BIT,vertex_shader_module);
    smm->Add(VK_SHADER_STAGE_FRAGMENT_BIT,fragment_shader_module);

    return(VK_NAMESPACE::CreateMaterial(device,smm));
}
VK_NAMESPACE_END
