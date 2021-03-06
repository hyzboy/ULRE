#include<hgl/graph/shader/ShaderResource.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/VKFormat.h>
#include<hgl/type/Map.h>

VK_NAMESPACE_BEGIN

    #define AccessByPointer(data,type)  *(type *)data;data+=sizeof(type);

    namespace
    {
        MapObject<OSString,ShaderResource> shader_resource_by_filename;

        constexpr char      SHADER_FILE_HEADER[]    ="Shader\x1A";
        constexpr uint      SHADER_FILE_HEADER_BYTES=sizeof(SHADER_FILE_HEADER)-1;

        constexpr uint32    SHADER_FILE_MIN_SIZE    =SHADER_FILE_HEADER_BYTES
                                                    +1                         //version
                                                    +sizeof(uint32)            //shader flag
                                                    +sizeof(uint32)            //spv_size
                                                    +1                         //input states count
                                                    +1;                        //output states count

        const uint8 *LoadShaderStages(ShaderStageList &ss_list,const uint8 *data)
        {
            const uint count=*data++;

            if(count<=0)
                return(data);

            const uint32 total_bytes=AccessByPointer(data,uint32);

            int str_len;

            ShaderStage *ss;

            for(uint i=0;i<count;i++)
            {
                ss=new ShaderStage;

                ss->location=*data++;
                ss->type.basetype=(VertexAttribBaseType)*data++;
                ss->type.vec_size=*data++;

                ss->format=VK_NAMESPACE::GetVulkanFormat(&(ss->type));

                str_len=*data++;
                ss->name.SetString((char *)data,str_len);
                data+=str_len;

                ss->binding=i;

                ss_list.Add(ss);
            }

            return data;
        }

        const uint8 *LoadShaderDescriptor(const uint8_t version,ShaderDescriptorList *sd_list,const uint8 *data)
        {   
            const uint32 total_bytes=AccessByPointer(data,uint32);

            const uint count=*data++;

            uint str_len;

            sd_list->binding_list.PreMalloc(count);
            sd_list->set_list.PreMalloc(count);

            for(uint i=0;i<count;i++)
            {
                if(version>=1)
                    sd_list->set_list.Add(*data++);
                else
                    sd_list->set_list.Add(0);

                sd_list->binding_list.Add(*data++);
                str_len=*data++;
                sd_list->name_list.Add(AnsiString((char *)data,str_len));
                data+=str_len;
            }

            return data;
        }
    }//namespcae

    ShaderResource::ShaderResource(const VkShaderStageFlagBits &flag,const void *sd,const uint32 size)
    {
        stage_flag=flag;
        spv_data=sd;
        spv_size=size;            
    }

    const os_char *ShaderStageName[]=
    {
        OS_TEXT("vert"),
        OS_TEXT("tesc"),
        OS_TEXT("tese"),
        OS_TEXT("geom"),
        OS_TEXT("frag"),
        OS_TEXT("comp"),
        OS_TEXT("task"),
        OS_TEXT("mesh")
    };
    
    const os_char *ShaderResource::GetStageName() const
    {
        switch(stage_flag)
        {
            case VK_SHADER_STAGE_VERTEX_BIT:                    return ShaderStageName[0];
            case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:      return ShaderStageName[1];
            case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:   return ShaderStageName[2];
            case VK_SHADER_STAGE_GEOMETRY_BIT:                  return ShaderStageName[3];
            case VK_SHADER_STAGE_FRAGMENT_BIT:                  return ShaderStageName[4];
            case VK_SHADER_STAGE_COMPUTE_BIT:                   return ShaderStageName[5];
            case VK_SHADER_STAGE_TASK_BIT_NV:                   return ShaderStageName[6];
            case VK_SHADER_STAGE_MESH_BIT_NV:                   return ShaderStageName[7];
            default:                                            return nullptr;
        }
    }

    const ShaderStage *ShaderResource::GetStageInput(const AnsiString &name) const
    {
        const int count=stage_inputs.GetCount();
        ShaderStage **ss=stage_inputs.GetData();

        for(int i=0;i<count;i++)
        {
            if(name==(*ss)->name)
                return *ss;

            ++ss;
        }

        return nullptr;
    }

    const int ShaderResource::GetStageInputBinding(const AnsiString &name) const
    {
        const int count=stage_inputs.GetCount();
        ShaderStage **ss=stage_inputs.GetData();

        for(int i=0;i<count;i++)
        {
            if(name==(*ss)->name)
                return i;

            ++ss;
        }

        return -1;
    }
    
    const int ShaderResource::GetBinding(VkDescriptorType desc_type,const AnsiString &name)const
    {
        if(desc_type>=VK_DESCRIPTOR_TYPE_RANGE_SIZE)return -1;
        if(name.IsEmpty())return -1;

        const ShaderDescriptorList *sdl=descriptor_list+(size_t)desc_type;

        const int index=sdl->name_list.Find(name);

        uint binding;

        if(sdl->binding_list.Get(index,binding))
            return binding;
        else
            return -1;
    }

    ShaderResource *LoadShaderResource(const uint8 *origin_filedata,const int64 filesize,bool include_file_header)
    {
        if(!origin_filedata)return(nullptr);

        const uint8 *filedata=origin_filedata;
        const uint8 *file_end=filedata+filesize;

        if(include_file_header)
        {
            if(filesize<SHADER_FILE_MIN_SIZE
             ||memcmp(origin_filedata,SHADER_FILE_HEADER,SHADER_FILE_HEADER_BYTES))
            {
                delete[] origin_filedata;
                return(nullptr);
            }

            filedata+=SHADER_FILE_HEADER_BYTES;
        }

        uint8                   version;
        VkShaderStageFlagBits   flag;
        uint32                  spv_size;
        uint32                  desc_type;

        version =AccessByPointer(filedata,uint8);
        flag    =(const VkShaderStageFlagBits)AccessByPointer(filedata,uint32);
        spv_size=AccessByPointer(filedata,uint32);

        ShaderResource *sr=new ShaderResource(flag,filedata,spv_size);

        filedata+=spv_size;

        filedata=LoadShaderStages(sr->GetStageInputs(),filedata);
        filedata=LoadShaderStages(sr->GetStageOutputs(),filedata);

        while(filedata<file_end)
        {
            desc_type=AccessByPointer(filedata,uint32);

            filedata=LoadShaderDescriptor(version,sr->GetDescriptorList((VkDescriptorType)desc_type),filedata);
        }

        return sr;
    }

    ShaderResource *LoadShaderResoruce(const OSString &filename)
    {
        ShaderResource *sr;

        if(shader_resource_by_filename.Get(filename,sr))
            return sr;

        int64 filesize;
        uint8 *filedata=(uint8 *)filesystem::LoadFileToMemory(filename+OS_TEXT(".shader"),filesize);

        if(!filedata)
        {        
            shader_resource_by_filename.Add(filename,nullptr);
            return(nullptr);
        }

        AutoDeleteArray<uint8> origin_filedata(filedata,filesize);

        sr=LoadShaderResource(origin_filedata,filesize,true);

        shader_resource_by_filename.Add(filename,sr);
        return sr;
    }
VK_NAMESPACE_END
