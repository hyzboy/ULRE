#include<hgl/vk/VKShaderDescriptorSet.h>

namespace hgl::graph{
/**
* 添加一个描述符
*/
ShaderDescriptor *ShaderDescriptorSet::AddDescriptor(uint32_t ssb,ShaderDescriptor *new_sd)
{
    ShaderDescriptor *sd;

    if(descriptor_map.Get(new_sd->name,sd))
    {
        delete new_sd;
        sd->stage_flag|=ssb;
        return(sd);
    }
    else
    {
        new_sd->set_type=set_type;
        new_sd->stage_flag=ssb;

        descriptor_map.Add(new_sd->name,new_sd);

        count++;

        return(new_sd);
    }
}
}//namespace hgl::graph
