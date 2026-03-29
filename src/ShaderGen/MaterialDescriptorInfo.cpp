#include<hgl/shadergen/MaterialDescriptorInfo.h>
#include<unordered_set>

namespace hgl{namespace graph{
MaterialDescriptorInfo::MaterialDescriptorInfo()
{
    int set_type=(int)DescriptorSetType::BEGIN_RANGE;

    for(auto &p:desc_set_array)
    {
        p.set_type=(DescriptorSetType)set_type;

        ++set_type;

        p.set=-1;
        p.count=0;
    }

    descriptor_count=0;
}

MaterialDescriptorInfo::~MaterialDescriptorInfo()
{
    std::unordered_set<ShaderDescriptor *> released;

    for(auto &set:desc_set_array)
    {
        auto release_descriptor = [&](ShaderDescriptor *sd)
        {
            if(!sd)
                return;

            if(released.insert(sd).second)
                delete sd;
        };

        for(auto *d:set.ubo_descriptor_map)
            release_descriptor(d);

        for(auto *d:set.ssbo_descriptor_map)
            release_descriptor(d);

        for(auto *d:set.texture_descriptor_map)
            release_descriptor(d);

        for(auto *d:set.texture_sampler_descriptor_map)
            release_descriptor(d);

        for(auto &p:set.ubo_descriptor_map) p=nullptr;
        for(auto &p:set.ssbo_descriptor_map) p=nullptr;
        for(auto &p:set.texture_descriptor_map) p=nullptr;
        for(auto &p:set.texture_sampler_descriptor_map) p=nullptr;
        set.count=0;
        set.set=-1;
    }

    for(auto &p:ubo_by_semantic)
        p=nullptr;

    for(auto &p:ssbo_by_semantic)
        p=nullptr;

    for(auto &p:texture_by_slot)
        p=nullptr;

    for(auto &p:texture_sampler_by_slot)
        p=nullptr;
}

const UBODescriptor *MaterialDescriptorInfo::AddUBO(uint32_t ssb,DescriptorSetType set_type,UBODescriptor *sd)
{
    RANGE_CHECK_RETURN_NULLPTR(set_type);
    if(!sd)return(nullptr);

    ShaderDescriptorSet *sds=desc_set_array+(size_t)set_type;

    UBODescriptor *obj=sds->AddUBO(ssb,sd);

    {
        const auto sem = obj->semantic;
        if(RangeCheck(sem))
            ubo_by_semantic[size_t(sem)] = obj;
    }

    return obj;
}

const SSBODescriptor *MaterialDescriptorInfo::AddSSBO(uint32_t ssb,DescriptorSetType set_type,SSBODescriptor *sd)
{
    RANGE_CHECK_RETURN_NULLPTR(set_type);
    if(!sd)return(nullptr);

    ShaderDescriptorSet *sds=desc_set_array+(size_t)set_type;

    SSBODescriptor *obj=sds->AddSSBO(ssb,sd);

    {
        const auto sem = obj->semantic;
        if(RangeCheck(sem))
            ssbo_by_semantic[size_t(sem)] = obj;
    }

    return obj;
}

const TextureDescriptor *MaterialDescriptorInfo::AddTexture(uint32_t shader_stage_flag_bits,DescriptorSetType set_type,TextureDescriptor *sd)
{
    RANGE_CHECK_RETURN_NULLPTR(set_type);
    if(!sd)return(nullptr);

    ShaderDescriptorSet *sds=desc_set_array+(size_t)set_type;

    TextureDescriptor *obj=sds->AddTexture(shader_stage_flag_bits,sd);

    {
        mtl::SamplerSlot slot = mtl::SamplerSlot::BaseColor;
        if(mtl::TryGetSlotFromDescriptorName(obj->name,slot))
            texture_by_slot[size_t(slot)] = obj;
    }
    return obj;
}

const TextureSamplerDescriptor *MaterialDescriptorInfo::AddTextureSampler(uint32_t ssb,DescriptorSetType set_type,TextureSamplerDescriptor *sd)
{
    RANGE_CHECK_RETURN_NULLPTR(set_type);
    if(!sd)return(nullptr);

    ShaderDescriptorSet *sds=desc_set_array+(size_t)set_type;

    TextureSamplerDescriptor *obj=sds->AddTextureSampler(ssb,sd);

    {
        mtl::SamplerSlot slot = mtl::SamplerSlot::BaseColor;
        if(mtl::TryGetSlotFromDescriptorName(obj->name,slot))
            texture_sampler_by_slot[size_t(slot)] = obj;
    }
    return obj;
}

UBODescriptor *MaterialDescriptorInfo::GetUBO(mtl::UBODescriptorSemantic semantic)
{
    if(!RangeCheck(semantic))
        return nullptr;

    return ubo_by_semantic[size_t(semantic)];
}

SSBODescriptor *MaterialDescriptorInfo::GetSSBO(mtl::SSBODescriptorSemantic semantic)
{
    if(!RangeCheck(semantic))
        return nullptr;

    return ssbo_by_semantic[size_t(semantic)];
}

void MaterialDescriptorInfo::Resort()
{
    descriptor_count=0;

    int set=0;

    for(auto &p:desc_set_array)
    {
        if(p.count<=0)
            continue;

        descriptor_count+=p.count;
        p.set=set;

        // Array indices are ascending enum order — exactly the desired binding order.
        int binding=0;

        for(auto *d:p.ubo_descriptor_map)
            if(d) { d->set=set; d->binding=binding++; }

        for(auto *d:p.ssbo_descriptor_map)
            if(d) { d->set=set; d->binding=binding++; }

        for(auto *d:p.texture_descriptor_map)
            if(d) { d->set=set; d->binding=binding++; }

        for(auto *d:p.texture_sampler_descriptor_map)
            if(d) { d->set=set; d->binding=binding++; }

        ++set;
    }
}
}}//namespace hgl::graph
