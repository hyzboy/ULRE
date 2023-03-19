#include<hgl/graph/VKShaderModule.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKShaderModuleMap.h>
#include<hgl/graph/VKShaderResource.h>
#include<hgl/graph/VKMaterialDescriptorSets.h>
#include<hgl/graph/VKVertexInput.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/VKRenderResource.h>
#include<hgl/io/ConstBufferReader.h>
#include<hgl/shadergen/MaterialCreateInfo.h>

VK_NAMESPACE_BEGIN

const ShaderModule *RenderResource::CreateShaderModule(const OSString &filename,VkShaderStageFlagBits shader_stage,const uint32_t *spv_data,const size_t spv_size)
{
    if(!device)return(nullptr);
    if(filename.IsEmpty())return(nullptr);
    if(!spv_data)return(nullptr);
    if(spv_size<4)return(nullptr);

    ShaderModule *sm;

    if(shader_module_by_name.Get(filename,sm))
        return sm;

    sm=device->CreateShaderModule(shader_stage,spv_data,spv_size);

    shader_module_by_name.Add(filename,sm);

    #ifdef _DEBUG
        {
            auto da=device->GetDeviceAttribute();
            const UTF8String sn=ToUTF8String(filename);
            
            if(da->debug_maker)
                da->debug_maker->SetShaderModule(*sm,sn);

            if(da->debug_utils)
                da->debug_utils->SetShaderModule(*sm,sn);
        }
    #endif//_DEBUG

    return sm;
}

void LoadShaderDescriptor(io::ConstBufferReader &cbr,ShaderDescriptor *sd_list,const uint count,const uint8 ver)
{
    ShaderDescriptor *sd=sd_list;

    for(uint i=0;i<count;i++)
    {
        cbr.CastRead<uint8>(sd->desc_type);
        cbr.ReadTinyString(sd->name);

        if(ver==3)
            cbr.CastRead<uint8>(sd->set_type);
        else
        if(ver==2)      //以下是旧的，未来不用了，现仅保证能运行
        {
            if(sd->name[0]=='g')sd->set_type=DescriptorSetType::Global;else
            if(sd->name[0]=='m')sd->set_type=DescriptorSetType::PerMaterial;else
            if(sd->name[0]=='r')sd->set_type=DescriptorSetType::PerObject;else
                sd->set_type=DescriptorSetType::PerFrame;
        }

        cbr.Read(sd->set);
        cbr.Read(sd->binding);
        cbr.Read(sd->stage_flag);

        if(sd->set_type==DescriptorSetType::PerObject)
        {
            if(sd->desc_type==VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)sd->desc_type=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;else
            if(sd->desc_type==VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)sd->desc_type=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
        }

        ++sd;
    }
}

ShaderResource *LoadShaderResource(io::ConstBufferReader &cbr);

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

    io::ConstBufferReader cbr(filedata,filesize);

    if(memcmp(cbr.CurPointer(),MaterialFileHeader,MaterialFileHeaderLength)!=0)
        return(nullptr);

    cbr.Skip(MaterialFileHeaderLength);

    uint8 ver;

    cbr.Read(ver);

    if(ver<2||ver>3)
        return(nullptr);

    uint32_t shader_bits;

    cbr.Read(shader_bits);

    const uint count=GetShaderCountByBits(shader_bits);
    uint32_t size;

    ShaderResource *sr;
    const ShaderModule *sm;
    
    bool result=true;
    ShaderModuleMap *smm=new ShaderModuleMap;
    VertexInput *vertex_input=nullptr;

    OSString shader_name;

    for(uint i=0;i<count;i++)
    {
        cbr.Read(size);

        sr=LoadShaderResource(io::ConstBufferReader(cbr,size));     //有一个stage output没读，放弃了，所以不能直接用上一级的cbr
        cbr.Skip(size);

        if(sr)
        {
            shader_name=filename+OS_TEXT("?")+ToOSString(sr->GetStageName());

            sm=CreateShaderModule(shader_name,sr->GetStage(),sr->GetSPVData(),sr->GetSPVSize());

            if(sm)
            {
                if(smm->Add(sm))
                {
                    if(sr->GetStage()==VK_SHADER_STAGE_VERTEX_BIT)
                        vertex_input=new VertexInput(sr->GetInputs());

                    continue;
                }
            }
        }   
    
        result=false;
        break;
    }

    const UTF8String mtl_name=ToUTF8String(filename);

    MaterialDescriptorSets *mds=nullptr;
    {
        uint8 count;
        cbr.Read(count);

        if(count>0)
        {
            ShaderDescriptor *sd_list=new ShaderDescriptor[count];

            LoadShaderDescriptor(cbr,sd_list,count,ver);
        
            mds=new MaterialDescriptorSets(mtl_name,sd_list,count);

            delete[] sd_list;
        }
    }

    if(result&&vertex_input)
    {
        mtl=device->CreateMaterial(mtl_name,smm,mds,vertex_input);
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

Material *RenderResource::CreateMaterial(const hgl::shadergen::MaterialCreateInfo *mci)
{
    if(!mci)
        return(nullptr);

    SHADERGEN_NAMESPACE_USING

    const uint count=GetShaderCountByBits(mci->GetShaderStage());
    const ShaderModule *sm;

    ShaderModuleMap *smm=new ShaderModuleMap;
    VertexInput *vertex_input=nullptr;

    const OSString mtl_name=ToOSString(mci->GetName());

    const ShaderCreateInfoVertex *vert=mci->GetVS();

    if(vert)
    {    
        sm=CreateShaderModule(  mtl_name+OS_TEXT("?Vertex"),
                                VK_SHADER_STAGE_VERTEX_BIT,
                                vert->GetSPVData(),vert->GetSPVSize());

        if(sm)
        {
            if(smm->Add(sm))
                vertex_input=new VertexInput(vert->GetInput());
        }

        smm->Add(sm);
    }

    const ShaderCreateInfoGeometry *geom=mci->GetGS();

    if(geom)
    {
        sm=CreateShaderModule(  mtl_name+OS_TEXT("?Geometry"),
                                VK_SHADER_STAGE_GEOMETRY_BIT,
                                geom->GetSPVData(),geom->GetSPVSize());

        smm->Add(sm);
    }

    const ShaderCreateInfoFragment *frag=mci->GetFS();

    if(frag)
    {
        sm=CreateShaderModule(  mtl_name+OS_TEXT("?Fragment"),
                                VK_SHADER_STAGE_FRAGMENT_BIT,
                                frag->GetSPVData(),frag->GetSPVSize());

        smm->Add(sm);
    }


}
VK_NAMESPACE_END
