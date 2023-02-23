#include<hgl/graph/VKShaderResource.h>
#include<hgl/graph/VKFormat.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/type/Map.h>
#include<hgl/io/ConstBufferReader.h>

VK_NAMESPACE_BEGIN

    namespace
    {
        ObjectMap<OSString,ShaderResource> shader_resource_by_filename;

        const bool LoadShaderStages(ShaderStageList &ss_list,io::ConstBufferReader &cbr)
        {
            uint count;

            cbr.CastRead<uint8>(count);

            if(count<=0)
                return(false);

            ShaderStage *ss;

            for(uint i=0;i<count;i++)
            {
                ss=new ShaderStage;

                cbr.Read(ss->location);
                cbr.CastRead<uint8>(ss->type.basetype);
                cbr.CastRead<uint8>(ss->type.vec_size);

                cbr.ReadTinyString(ss->name);

                ss_list.Add(ss);
            }

            return true;
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

    ShaderResource *LoadShaderResource(io::ConstBufferReader &cbr)
    {
        VkShaderStageFlagBits   flag;
        uint32                  spv_size;

        cbr.CastRead<uint32>(flag);
        cbr.Read(spv_size);

        ShaderResource *sr=new ShaderResource(flag,cbr.CurPointer(),spv_size);

        cbr.Skip(spv_size);

        LoadShaderStages(sr->GetStageInputs(),cbr);
//        LoadShaderStages(sr->GetStageOutputs(),cbr);

        return sr;
    }
VK_NAMESPACE_END
