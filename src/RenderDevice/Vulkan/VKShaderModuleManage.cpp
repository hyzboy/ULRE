#include<hgl/graph/vulkan/VKShaderModuleManage.h>
#include<hgl/graph/vulkan/VKShaderModule.h>
#include<hgl/graph/vulkan/VKMaterial.h>
#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/graph/vulkan/ShaderModuleMap.h>
#include<hgl/graph/shader/ShaderResource.h>
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

const ShaderModule *ShaderModuleManage::CreateShader(ShaderResource *sr)
{
    if(!sr)
        return(nullptr);

    VkPipelineShaderStageCreateInfo *shader_stage=new VkPipelineShaderStageCreateInfo;
    shader_stage->sType                 =VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage->pNext                 =nullptr;
    shader_stage->pSpecializationInfo   =nullptr;
    shader_stage->flags                 =0;
    shader_stage->stage                 =sr->GetStage();
    shader_stage->pName                 ="main";

    VkShaderModuleCreateInfo moduleCreateInfo;
    moduleCreateInfo.sType      =VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleCreateInfo.pNext      =nullptr;
    moduleCreateInfo.flags      =0;
    moduleCreateInfo.codeSize   =sr->GetCodeSize();
    moduleCreateInfo.pCode      =sr->GetCode();

    if(vkCreateShaderModule(*device,&moduleCreateInfo,nullptr,&(shader_stage->module))!=VK_SUCCESS)
        return(nullptr);

    ShaderModule *sm;

    if(sr->GetStage()==VK_SHADER_STAGE_VERTEX_BIT)
        sm=new VertexShaderModule(*device,shader_count,shader_stage,sr);
    else
        sm=new ShaderModule(*device,shader_count,shader_stage,sr);

    shader_list.Add(shader_count,sm);

    ++shader_count;

    return sm;
}

const ShaderModule *ShaderModuleManage::CreateShader(const VkShaderStageFlagBits shader_stage_bit,const OSString &filename)
{
    ShaderResource *shader_resource=LoadShaderResoruce(filename);

    if(!shader_resource)return(nullptr);

    return CreateShader(shader_resource);
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

void ShaderModuleManage::Free(ShaderModuleMap *smm)
{
    const int count=smm->GetCount();
    
    auto **it=smm->GetDataList();

    for(int i=0;i<count;i++)
    {
        ReleaseShader((*it)->right);

        ++it;
    }

    delete smm;
}

Material *ShaderModuleManage::CreateMaterial(ShaderModuleMap *smm)
{
    Material *mtl=VK_NAMESPACE::CreateMaterial(device,smm);

    if(mtl)return(mtl);

    Free(smm);
    return(nullptr);
}

Material *ShaderModuleManage::CreateMaterial(const VertexShaderModule *vertex_shader_module,const ShaderModule *fragment_shader_module)
{
    if(!vertex_shader_module||!fragment_shader_module)
        return(nullptr);

    if(!vertex_shader_module->IsVertex())return(nullptr);
    if(!fragment_shader_module->IsFragment())return(nullptr);

    ShaderModuleMap *smm=new ShaderModuleMap;

    smm->Add(vertex_shader_module);
    smm->Add(fragment_shader_module);

    return CreateMaterial(smm);
}

Material *ShaderModuleManage::CreateMaterial(const VertexShaderModule *vertex_shader_module,const ShaderModule *geometry_shader_module,const ShaderModule *fragment_shader_module)
{
    if(!vertex_shader_module
     ||!geometry_shader_module
     ||!fragment_shader_module)
        return(nullptr);

    if(!vertex_shader_module->IsVertex())return(nullptr);
    if(!geometry_shader_module->IsGeometry())return(nullptr);
    if(!fragment_shader_module->IsFragment())return(nullptr);

    ShaderModuleMap *smm=new ShaderModuleMap;

    smm->Add(vertex_shader_module);
    smm->Add(geometry_shader_module);
    smm->Add(fragment_shader_module);

    return CreateMaterial(smm);
}

Material *ShaderModuleManage::CreateMaterial(const OSString &vertex_shader_filename,const OSString &fragment_shader_filename)
{
    const ShaderModule *vs=CreateShader(VK_SHADER_STAGE_VERTEX_BIT,vertex_shader_filename);

    if(!vs)
        return(nullptr);

    const ShaderModule *fs=CreateShader(VK_SHADER_STAGE_FRAGMENT_BIT,fragment_shader_filename);

    if(!fs)
    {
        ReleaseShader(vs);
        return(nullptr);
    }

    return(CreateMaterial((VertexShaderModule *)vs,fs));
}

Material *ShaderModuleManage::CreateMaterial(const OSString &vertex_shader_filename,const OSString &geometry_shader_filename,const OSString &fragment_shader_filename)
{
    const ShaderModule *vs=CreateShader(VK_SHADER_STAGE_VERTEX_BIT,vertex_shader_filename);

    if(!vs)
        return(nullptr);

    const ShaderModule *gs=CreateShader(VK_SHADER_STAGE_GEOMETRY_BIT,geometry_shader_filename);

    if(!gs)
    {
        ReleaseShader(vs);
        return(nullptr);
    }

    const ShaderModule *fs=CreateShader(VK_SHADER_STAGE_FRAGMENT_BIT,fragment_shader_filename);

    if(!fs)
    {
        ReleaseShader(gs);
        ReleaseShader(vs);
        return(nullptr);
    }

    return(CreateMaterial((VertexShaderModule *)vs,gs,fs));
}

Material *ShaderModuleManage::CreateMaterial(const OSString &filename)
{
    constexpr char MaterialFileHeader[]=u8"Material\x1A";
    constexpr uint MaterialFileHeaderLength=sizeof(MaterialFileHeader)-1;

    int64 filesize;
    AutoDeleteArray<uint8> origin_filedata=(uint8 *)filesystem::LoadFileToMemory(filename+OS_TEXT(".material"),filesize);

    if(filesize<MaterialFileHeaderLength)
        return(nullptr);

    const uint8 *sp=origin_filedata;
    int64 left=filesize;

    if(memcmp(sp,MaterialFileHeader,MaterialFileHeaderLength)!=0)
        return(nullptr);

    sp+=MaterialFileHeaderLength;
    left-=MaterialFileHeaderLength;

    const uint8 ver=*sp;
    ++sp;
    --left;

    if(ver!=1)
        return(nullptr);

    const uint32_t shader_bits=*(uint32_t *)sp;
    sp+=sizeof(uint32_t);
    left-=sizeof(uint32_t);

    const uint count=GetShaderCountByBits(shader_bits);
    uint32_t size;

    ShaderResource *sr;
    const ShaderModule *sm;
    
    bool result=true;
    ShaderModuleMap *smm=new ShaderModuleMap;

    for(uint i=0;i<count;i++)
    {
        size=*(uint32_t *)sp;
        sp+=sizeof(uint32_t);
        left-=sizeof(uint32_t);

        sr=LoadShaderResource(sp,size,false);
        sp+=size;
        left-=size;

        if(sr)
        {
            sm=CreateShader(sr);

            if(sm)
            {
                if(smm->Add(sm))
                    continue;
            }
        }

        result=false;
        break;
    }

    if(result)
        return CreateMaterial(smm);
    
    Free(smm);
    return(nullptr);
}
VK_NAMESPACE_END
