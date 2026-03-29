#include<hgl/shadergen/MaterialDescriptorInfo.h>
#include<vector>
#include<algorithm>
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

        for(auto &kv:set.ubo_descriptor_map)
            release_descriptor(kv.second);

        for(auto &kv:set.ssbo_descriptor_map)
            release_descriptor(kv.second);

        for(auto &kv:set.texture_descriptor_map)
            release_descriptor(kv.second);

        for(auto &kv:set.texture_sampler_descriptor_map)
            release_descriptor(kv.second);

        set.ubo_descriptor_map.clear();
        set.ssbo_descriptor_map.clear();
        set.texture_descriptor_map.clear();
        set.texture_sampler_descriptor_map.clear();
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

        // Collect descriptor pointers
        std::vector<ShaderDescriptor *> ordered;
        ordered.reserve(static_cast<size_t>(p.count));

        for(const auto &kv:p.ubo_descriptor_map)
            if(kv.second)
                ordered.emplace_back(kv.second);

        for(const auto &kv:p.ssbo_descriptor_map)
            if(kv.second)
                ordered.emplace_back(kv.second);

        for(const auto &kv:p.texture_descriptor_map)
            if(kv.second)
                ordered.emplace_back(kv.second);

        for(const auto &kv:p.texture_sampler_descriptor_map)
            if(kv.second)
                ordered.emplace_back(kv.second);

        // Sort: builtin semantics first in enum order, then unknowns/custom by name
        std::sort(ordered.begin(),ordered.end(),
            [](const ShaderDescriptor *a,const ShaderDescriptor *b)->bool
            {
                const auto *a_ubo=dynamic_cast<const UBODescriptor *>(a);
                const auto *b_ubo=dynamic_cast<const UBODescriptor *>(b);

                if(a_ubo&&b_ubo)
                {
                    const bool a_builtin=RangeCheck(a_ubo->semantic);
                    const bool b_builtin=RangeCheck(b_ubo->semantic);
                    if(a_builtin!=b_builtin)
                        return a_builtin;
                    if(a_builtin)
                        return uint8_t(a_ubo->semantic)<uint8_t(b_ubo->semantic);
                    return std::strcmp(a->name,b->name)<0;
                }

                const auto *a_ssbo=dynamic_cast<const SSBODescriptor *>(a);
                const auto *b_ssbo=dynamic_cast<const SSBODescriptor *>(b);

                if(a_ssbo&&b_ssbo)
                {
                    const bool a_builtin=RangeCheck(a_ssbo->semantic);
                    const bool b_builtin=RangeCheck(b_ssbo->semantic);
                    if(a_builtin!=b_builtin)
                        return a_builtin;
                    if(a_builtin)
                        return uint8_t(a_ssbo->semantic)<uint8_t(b_ssbo->semantic);
                    return std::strcmp(a->name,b->name)<0;
                }

                const bool a_builtin=false;
                const bool b_builtin=false;
                if(a_builtin!=b_builtin)
                    return a_builtin;
                return std::strcmp(a->name,b->name)<0;
            });

        int i=0;
        for(auto *sd:ordered)
        {
            sd->set=set;
            sd->binding=i;
            ++i;
        }

        ++set;
    }
}
}}//namespace hgl::graph
