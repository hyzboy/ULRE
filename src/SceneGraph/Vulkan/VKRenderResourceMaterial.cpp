#include<hgl/graph/VKShaderModule.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKShaderModuleMap.h>
#include<hgl/graph/shader/ShaderResource.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/VKRenderResource.h>

VK_NAMESPACE_BEGIN

const ShaderModule *RenderResource::CreateShaderModule(const OSString &filename,ShaderResource *shader_resource)
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

const bool LoadShaderDescriptor(ShaderResource *sr,const uint8 *data)
{   
    const uint8 count=*data++;

    if(count<=0)return(true);

    ShaderDescriptorList *sd_list;
    const VkDescriptorType desc_type;



    uint str_len;

    sd_list->SetCount(count);

    ShaderDescriptor *sd=sd_list->GetData();

    for(uint i=0;i<count;i++)
    {
        sd->set=*data++;
        sd->binding=*data++;
        str_len=*data++;

        memcpy(sd->name,(char *)data,str_len);
        sd->name[str_len]=0;
        data+=str_len;

        sd->set_type=CheckDescriptorSetType(sd->name);

        if(sd->set_type==DescriptorSetType::Renderable)
        {
            if(desc_type==VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)sd->desc_type=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;else
            if(desc_type==VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)sd->desc_type=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;else
                                                            sd->desc_type=desc_type;
        }
        else
        {
            sd->desc_type=desc_type;
        }

        ++sd;
    }

    return data;
}

Material *RenderResource::CreateMaterial(const OSString &filename)
{
    Material *mtl;

    if(material_by_name.Get(filename,mtl))
        return mtl;

    constexpr char MaterialFileHeader[]=u8"Material\x1A";
    constexpr uint MaterialFileHeaderLength=sizeof(MaterialFileHeader)-1;

    int64 filesize;
    uint8 *filedata=(uint8 *)filesystem::LoadFileToMemory(filename+OS_TEXT(".material"),filesize);

    if(!filedata)
    {
        material_by_name.Add(filename,nullptr);
        return(nullptr);
    }

    AutoDeleteArray<uint8> origin_filedata(filedata,filesize);

    if(filesize<MaterialFileHeaderLength)
        return(nullptr);

    const uint8 *sp=origin_filedata;
    const uint8 *end=sp+filesize;

    if(memcmp(sp,MaterialFileHeader,MaterialFileHeaderLength)!=0)
        return(nullptr);

    sp+=MaterialFileHeaderLength;

    const uint8 ver=*sp;
    ++sp;

    if(ver!=2)
        return(nullptr);

    const uint32_t shader_bits=*(uint32_t *)sp;
    sp+=sizeof(uint32_t);

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

        sr=LoadShaderResource(sp,size);
        sp+=size;

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

    if(!LoadShaderDescriptor(sr,filedata))
        result=false;

    if(result)
    {
        mtl=device->CreateMaterial(ToUTF8String(filename),smm);
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
