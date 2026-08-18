#include<hgl/mtl/DescriptorSetLayoutAllocator.h>
namespace hgl{namespace graph::mtl{
    using namespace hgl::graph::mtl;
DescriptorSetLayoutAllocator::DescriptorSetLayoutAllocator()
{
    int set_type=(int)DescriptorSetType::BEGIN_RANGE;

    for(auto &p:desc_set_array)
    {
        p.set_type=(DescriptorSetType)set_type;

        ++set_type;

        p.set=-1;
        p.count=0;
    }
}

const UBODescriptor *DescriptorSetLayoutAllocator::AddUBO(uint32_t shader_stage_flag_bits,DescriptorSetType set_type,UBODescriptor *sd)
{
    RANGE_CHECK_RETURN_NULLPTR(set_type);
    if(!sd)return(nullptr);

    ShaderDescriptorSet *sds=desc_set_array+(size_t)set_type;

    ShaderDescriptor *obj=sds->AddDescriptor(shader_stage_flag_bits,sd);

    ubo_map[obj->name] = (UBODescriptor *)obj;
    return((UBODescriptor *)obj);
}

const SSBODescriptor *DescriptorSetLayoutAllocator::AddSSBO(uint32_t shader_stage_flag_bits,DescriptorSetType set_type,SSBODescriptor *sd)
{
    RANGE_CHECK_RETURN_NULLPTR(set_type);
    if(!sd)return(nullptr);

    ShaderDescriptorSet *sds=desc_set_array+(size_t)set_type;

    ShaderDescriptor *obj=sds->AddDescriptor(shader_stage_flag_bits,sd);

    ssbo_map[obj->name] = (SSBODescriptor *)obj;
    return((SSBODescriptor *)obj);
}

UBODescriptor *DescriptorSetLayoutAllocator::GetUBO(const std::string &name)
{
    const auto iter=ubo_map.find(name);
    if(iter!=ubo_map.end())
        return iter->second;

    return(nullptr);
}

SSBODescriptor *DescriptorSetLayoutAllocator::GetSSBO(const std::string &name)
{
    const auto iter=ssbo_map.find(name);
    if(iter!=ssbo_map.end())
        return iter->second;

    return(nullptr);
}
}}//namespace hgl::graph::mtl

