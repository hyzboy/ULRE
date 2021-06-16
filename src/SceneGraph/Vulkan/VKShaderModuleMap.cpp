#include<hgl/graph/VKShaderModuleMap.h>
#include<hgl/graph/VKShaderModule.h>

VK_NAMESPACE_BEGIN
bool ShaderModuleMap::Add(const ShaderModule *sm)
{
    if(!sm)return(false);

    const VkShaderStageFlagBits stage=sm->GetStage();

    if(this->KeyExist(stage))return(false);

    return this->Map::Add(stage,sm);
}

const int ShaderModuleMap::GetBinding(VkDescriptorType desc_type,const AnsiString &name)const
{
    if(desc_type<VK_DESCRIPTOR_TYPE_BEGIN_RANGE
     ||desc_type>VK_DESCRIPTOR_TYPE_END_RANGE
     ||name.IsEmpty())
        return(-1);

    int binding;
    const int shader_count=GetCount();

    const ShaderModule *sm;
    auto **itp=GetDataList();
    for(int i=0;i<shader_count;i++)
    {
        sm=(*itp)->right;
        binding=sm->GetBinding(desc_type,name);
        if(binding!=-1)
            return binding;

        ++itp;
    }

    return(-1);
}
VK_NAMESPACE_END