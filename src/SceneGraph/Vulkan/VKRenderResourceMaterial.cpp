#include<hgl/graph/VKShaderModule.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKShaderModuleMap.h>
#include<hgl/graph/VKShaderResource.h>
#include<hgl/graph/VKMaterialDescriptorSets.h>
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

void LoadShaderDescriptor(const uint8 *data,ShaderDescriptor *sd_list,const uint count)
{
    ShaderDescriptor *sd=sd_list;
    uint str_len;

    for(uint i=0;i<count;i++)
    {
        sd->desc_type=VkDescriptorType(*data++);

        {
            str_len=*data++;
            memcpy(sd->name,(char *)data,str_len);
            data+=str_len;
        }

        sd->set         =*data++;
        sd->binding     =*data++;
        sd->stage_flag  =*(uint32 *)data;
        data+=sizeof(uint32);

        sd->set_type=CheckDescriptorSetsType(sd->name);

        if(sd->set_type==DescriptorSetsType::Renderable)
        {
            if(sd->desc_type==VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)sd->desc_type=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;else
            if(sd->desc_type==VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)sd->desc_type=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
        }

        ++sd;
    }
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

    const UTF8String mtl_name=ToUTF8String(filename);

    MaterialDescriptorSets *mds=nullptr;
    {
        const uint8 count=*sp;
        ++sp;

        if(count>0)
        {
            ShaderDescriptor *sd_list=hgl_zero_new<ShaderDescriptor>(count);

            LoadShaderDescriptor(sp,sd_list,count);
        
            mds=new MaterialDescriptorSets(mtl_name,sd_list,count);
        }
    }

    if(result)
    {
        mtl=device->CreateMaterial(mtl_name,smm,mds);
        Add(mtl);
    }
    else
    {
        SAFE_CLEAR(mds);
        delete smm;
        mtl=nullptr;
    }
    
    material_by_name.Add(filename,mtl);    
    return(mtl);
}
VK_NAMESPACE_END
