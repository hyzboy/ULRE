#include<hgl/shadergen/ShaderDescriptorInfo.h>

namespace hgl{namespace graph{
ShaderDescriptorInfo::ShaderDescriptorInfo(ShaderStage flag_bit)
{
    stage_flag=flag_bit;

    mem_zero(push_constant);
}

ShaderDescriptorInfo::~ShaderDescriptorInfo()
{
    for(auto *p:const_value_list)
        delete p;
}

std::string ShaderDescriptorInfo::GetStageName()const
{
    switch(stage_flag)
    {
        case ShaderStage::Vertex: return "Vertex";
        case ShaderStage::TessControl: return "TessControl";
        case ShaderStage::TessEval: return "TessEval";
        case ShaderStage::Geometry: return "Geometry";
        case ShaderStage::Fragment: return "Fragment";
        case ShaderStage::Compute: return "Compute";
        case ShaderStage::Task: return "Task";
        case ShaderStage::Mesh: return "Mesh";
        case ShaderStage::ClusterCulling: return "ClusterCulling";
        default: return "Unknown";
    }
}

bool ShaderDescriptorInfo::AddUBO(DescriptorSetType type,const UBODescriptor *ubo)
{
    if(!ubo)
        return(false);

    ubo_list.push_back(ubo);
    return true;
}

bool ShaderDescriptorInfo::AddSSBO(DescriptorSetType type,const SSBODescriptor *ssbo)
{
    if(!ssbo)
        return(false);

    ssbo_list.push_back(ssbo);
    return true;
}

bool ShaderDescriptorInfo::AddTexture(DescriptorSetType type,const TextureDescriptor *sd)
{
    if(!sd)
        return(false);

    texture_list.push_back(sd);
    return true;
}

bool ShaderDescriptorInfo::AddTextureSampler(DescriptorSetType type,const TextureSamplerDescriptor *sampler)
{
    if(!sampler)
        return(false);

    texture_sampler_list.push_back(sampler);
    return true;
}

bool ShaderDescriptorInfo::AddConstValue(ConstValueDescriptor *sd)
{
    if(!sd)return(false);

    for(auto *p:const_value_list)
        if(p->name==sd->name)
            return(false);

    sd->constant_id=static_cast<int>(const_value_list.size());
    const_value_list.push_back(sd);
    return(true);
}

void ShaderDescriptorInfo::SetPushConstant(const std::string &name,uint8_t offset,uint8_t size)
{
    push_constant.name  =name;
    push_constant.offset=offset;
    push_constant.size  =size;
}
}}//namespace hgl::graph
