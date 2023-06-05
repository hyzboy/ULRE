#include<hgl/graph/VKShaderResource.h>
#include<hgl/graph/VKFormat.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/type/Map.h>
#include<hgl/io/ConstBufferReader.h>

VK_NAMESPACE_BEGIN
    ShaderResource::ShaderResource(const VkShaderStageFlagBits &flag,const uint32_t *sd,const uint32 size)
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
VK_NAMESPACE_END
