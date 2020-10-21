#include<hgl/graph/vulkan/VKShaderModule.h>
#include<hgl/graph/vulkan/VKMaterial.h>
#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/graph/vulkan/VKShaderModuleMap.h>
#include<hgl/graph/shader/ShaderResource.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/vulkan/VKDatabase.h>

VK_NAMESPACE_BEGIN

const ShaderModule *Database::CreateShaderModule(const OSString &filename,ShaderResource *shader_resource)
{
    if(!device)return(nullptr);
    if(filename.IsEmpty())return(nullptr);
    if(!shader_resource)return(nullptr);

    ShaderModule *sm;

    if(shader_module_by_name.Get(filename,sm))
        return sm;

    sm=device->CreateShaderModule(shader_resource);

    shader_module_by_name.Add(filename,sm);

    return sm;
}

const ShaderModule *Database::CreateShaderModule(const OSString &filename)
{
    ShaderModule *sm;

    if(shader_module_by_name.Get(filename,sm))
        return sm;

    ShaderResource *shader_resource=LoadShaderResoruce(filename);

    if(!shader_resource)return(nullptr);

    sm=device->CreateShaderModule(shader_resource);

    shader_module_by_name.Add(filename,sm);

    return sm;
}

Material *Database::CreateMaterial(const OSString &vertex_shader_filename,const OSString &fragment_shader_filename)
{
    const ShaderModule *vs=CreateShaderModule(vertex_shader_filename);

    if(!vs)
        return(nullptr);

    const ShaderModule *fs=CreateShaderModule(fragment_shader_filename);

    if(!fs)
        return(nullptr);

    return(device->CreateMaterial((VertexShaderModule *)vs,fs));
}

Material *Database::CreateMaterial(const OSString &vertex_shader_filename,const OSString &geometry_shader_filename,const OSString &fragment_shader_filename)
{
    const ShaderModule *vs=CreateShaderModule(vertex_shader_filename);

    if(!vs)
        return(nullptr);

    const ShaderModule *gs=CreateShaderModule(geometry_shader_filename);

    if(!gs)
        return(nullptr);

    const ShaderModule *fs=CreateShaderModule(fragment_shader_filename);

    if(!fs)
        return(nullptr);

    return(device->CreateMaterial((VertexShaderModule *)vs,gs,fs));
}

Material *Database::CreateMaterial(const OSString &filename)
{
    Material *mtl;

    if(material_by_name.Get(filename,mtl))
        return mtl;

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

    OSString shader_name;

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
            shader_name=filename+OS_TEXT("?")+OSString(sr->GetStageName());

            sm=CreateShaderModule(shader_name,sr);

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
    {
        mtl=device->CreateMaterial(smm);
        Add(mtl);
    }
    else
    {
        delete smm;
        mtl=nullptr;
    }
    
    material_by_name.Add(filename,mtl);    
    return(mtl);
}
VK_NAMESPACE_END
