#include<hgl/graph/VKShaderResource.h>
#include<hgl/graph/VKFormat.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/type/Map.h>

VK_NAMESPACE_BEGIN

    #define AccessByPointer(data,type)  *(type *)data;data+=sizeof(type);

    namespace
    {
        ObjectMap<OSString,ShaderResource> shader_resource_by_filename;

        const uint8 *LoadShaderStages(ShaderStageList &ss_list,const uint8 *data)
        {
            const uint count=*data++;

            if(count<=0)
                return(data);

            int str_len;

            ShaderStage *ss;

            for(uint i=0;i<count;i++)
            {
                ss=new ShaderStage;

                ss->location        =*data++;
                ss->type.basetype   =(VertexAttribBaseType)*data++;
                ss->type.vec_size   =*data++;

                str_len=*data++;
                ss->name.SetString((char *)data,str_len);
                data+=str_len;

                ss_list.Add(ss);
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

    ShaderResource *LoadShaderResource(const uint8 *origin_filedata,const int64 filesize)
    {
        if(!origin_filedata)return(nullptr);

        const uint8 *filedata=origin_filedata;
        const uint8 *file_end=filedata+filesize;

        VkShaderStageFlagBits   flag;
        uint32                  spv_size;

        flag    =(const VkShaderStageFlagBits)AccessByPointer(filedata,uint32);
        spv_size=AccessByPointer(filedata,uint32);

        ShaderResource *sr=new ShaderResource(flag,filedata,spv_size);

        filedata+=spv_size;

        filedata=LoadShaderStages(sr->GetStageInputs(),filedata);
        //filedata=LoadShaderStages(sr->GetStageOutputs(),filedata);

        return sr;
    }
VK_NAMESPACE_END
