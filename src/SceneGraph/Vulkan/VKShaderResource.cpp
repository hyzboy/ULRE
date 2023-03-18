#include<hgl/graph/VKShaderResource.h>
#include<hgl/graph/VKFormat.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/type/Map.h>
#include<hgl/io/ConstBufferReader.h>

VK_NAMESPACE_BEGIN

    namespace
    {
        ObjectMap<OSString,ShaderResource> shader_resource_by_filename;

        const bool LoadShaderStageAttributes(ShaderAttributeArray &sa_array,io::ConstBufferReader &cbr)
        {
            uint count;

            cbr.CastRead<uint8>(count);

            if(count<=0)
                return(false);

            Init(&sa_array,count);

            ShaderAttribute *ss=sa_array.items;

            for(uint i=0;i<count;i++)
            {
                cbr.Read(ss->location);
                cbr.CastRead<uint8>(ss->basetype);
                cbr.CastRead<uint8>(ss->vec_size);

                cbr.ReadTinyString(ss->name);

                ++ss;
            }

            return true;
        }
    }//namespcae

    ShaderResource::ShaderResource(const VkShaderStageFlagBits &flag,const void *sd,const uint32 size)
    {
        stage_flag=flag;
        spv_data=sd;
        spv_size=size;

        Init(stage_io);
    }

    ShaderResource::~ShaderResource()
    {
        Clear(stage_io);
    }

    const ShaderAttribute *ShaderResource::GetInput(const AnsiString &name) const
    {
        if(stage_io.input.count<=0)return(nullptr);

        const ShaderAttribute *ss=stage_io.input.items;

        for(uint i=0;i<stage_io.input.count;i++)
        {
            if(name==ss->name)
                return ss;

            ++ss;
        }

        return nullptr;
    }

    const int ShaderResource::GetInputBinding(const AnsiString &name) const
    {
        if(stage_io.input.count<=0)return(-1);

        const ShaderAttribute *ss=stage_io.input.items;

        for(uint i=0;i<stage_io.input.count;i++)
        {
            if(name==ss->name)
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

        LoadShaderStageAttributes(sr->GetInputs(),cbr);
//        LoadShaderStageAttributes(sr->GetStageOutputs(),cbr);

        return sr;
    }
VK_NAMESPACE_END
