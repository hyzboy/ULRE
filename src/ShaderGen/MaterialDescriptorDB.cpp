#include<hgl/shadergen/MaterialDescriptorDB.h>
#include<cassert>

namespace hgl{namespace graph{
MaterialDescriptorDB::MaterialDescriptorDB()
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

MaterialDescriptorDB::~MaterialDescriptorDB()
{
    for(auto &set:desc_set_array)
    {
        for(auto *d:set.ubo_descriptor_map)
        {
            if(!d) continue;
            assert(d != nullptr);
            delete d;
        }

        for(auto *d:set.ssbo_descriptor_map)
        {
            if(!d) continue;
            assert(d != nullptr);
            delete d;
        }

        for(auto *d:set.texture_descriptor_map)
        {
            if(!d) continue;
            assert(d != nullptr);
            delete d;
        }

        for(auto *d:set.texture_sampler_descriptor_map)
        {
            if(!d) continue;
            assert(d != nullptr);
            delete d;
        }

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

const UBODescriptor *MaterialDescriptorDB::AddUBO(uint32_t ssb,DescriptorSetType set_type,UBODescriptor *sd)
{
    return AddUBO(ssb,set_type,std::unique_ptr<UBODescriptor>(sd));
}

const UBODescriptor *MaterialDescriptorDB::AddUBO(uint32_t ssb,DescriptorSetType set_type,std::unique_ptr<UBODescriptor> sd)
{
    RANGE_CHECK_RETURN_NULLPTR(set_type);
    if(!sd)return(nullptr);

    ShaderDescriptorSet *sds=desc_set_array+(size_t)set_type;

    UBODescriptor *obj=sds->AddUBO(ssb,std::move(sd));

    if(!obj)
        return nullptr;

    {
        const auto sem = obj->semantic;
        if(RangeCheck(sem))
            ubo_by_semantic[size_t(sem)] = obj;
    }

    return obj;
}

const SSBODescriptor *MaterialDescriptorDB::AddSSBO(uint32_t ssb,DescriptorSetType set_type,SSBODescriptor *sd)
{
    return AddSSBO(ssb,set_type,std::unique_ptr<SSBODescriptor>(sd));
}

const SSBODescriptor *MaterialDescriptorDB::AddSSBO(uint32_t ssb,DescriptorSetType set_type,std::unique_ptr<SSBODescriptor> sd)
{
    RANGE_CHECK_RETURN_NULLPTR(set_type);
    if(!sd)return(nullptr);

    ShaderDescriptorSet *sds=desc_set_array+(size_t)set_type;

    SSBODescriptor *obj=sds->AddSSBO(ssb,std::move(sd));

    if(!obj)
        return nullptr;

    // VertexStreams SSBOs are addressed by binding index, not by semantic lookup;
    // skip ssbo_by_semantic to avoid corrupting the per-semantic index map.
    if (obj && set_type != DescriptorSetType::VertexStreams)
    {
        const auto sem = obj->semantic;
        if(RangeCheck(sem))
            ssbo_by_semantic[size_t(sem)] = obj;
    }

    return obj;
}

const TextureDescriptor *MaterialDescriptorDB::AddTexture(uint32_t shader_stage_flag_bits,DescriptorSetType set_type,TextureDescriptor *sd)
{
    return AddTexture(shader_stage_flag_bits,set_type,std::unique_ptr<TextureDescriptor>(sd));
}

const TextureDescriptor *MaterialDescriptorDB::AddTexture(uint32_t shader_stage_flag_bits,DescriptorSetType set_type,std::unique_ptr<TextureDescriptor> sd)
{
    RANGE_CHECK_RETURN_NULLPTR(set_type);
    if(!sd)return(nullptr);

    ShaderDescriptorSet *sds=desc_set_array+(size_t)set_type;

    TextureDescriptor *obj=sds->AddTexture(shader_stage_flag_bits,std::move(sd));

    if(!obj)
        return nullptr;

    {
        mtl::SamplerSlot slot = mtl::SamplerSlot::BaseColor;
        if(mtl::TryGetSlotFromDescriptorName(obj->name,slot))
            texture_by_slot[size_t(slot)] = obj;
    }
    return obj;
}

const TextureSamplerDescriptor *MaterialDescriptorDB::AddTextureSampler(uint32_t ssb,DescriptorSetType set_type,TextureSamplerDescriptor *sd)
{
    return AddTextureSampler(ssb,set_type,std::unique_ptr<TextureSamplerDescriptor>(sd));
}

const TextureSamplerDescriptor *MaterialDescriptorDB::AddTextureSampler(uint32_t ssb,DescriptorSetType set_type,std::unique_ptr<TextureSamplerDescriptor> sd)
{
    RANGE_CHECK_RETURN_NULLPTR(set_type);
    if(!sd)return(nullptr);

    ShaderDescriptorSet *sds=desc_set_array+(size_t)set_type;

    TextureSamplerDescriptor *obj=sds->AddTextureSampler(ssb,std::move(sd));

    if(!obj)
        return nullptr;

    {
        mtl::SamplerSlot slot = mtl::SamplerSlot::BaseColor;
        if(mtl::TryGetSlotFromDescriptorName(obj->name,slot))
            texture_sampler_by_slot[size_t(slot)] = obj;
    }
    return obj;
}

UBODescriptor *MaterialDescriptorDB::GetUBO(mtl::UBODescriptorSemantic semantic)
{
    if(!RangeCheck(semantic))
        return nullptr;

    return ubo_by_semantic[size_t(semantic)];
}

SSBODescriptor *MaterialDescriptorDB::GetSSBO(mtl::SSBODescriptorSemantic semantic)
{
    if(!RangeCheck(semantic))
        return nullptr;

    return ssbo_by_semantic[size_t(semantic)];
}

void MaterialDescriptorDB::Resort()
{
    descriptor_count=0;

    int set=0;

    for(auto &p:desc_set_array)
    {
        if(p.count<=0)
            continue;

        descriptor_count+=p.count;
        p.set=set;

        if (p.set_type == DescriptorSetType::VertexStreams)
        {
            // Vertex stream SSBOs use the array index directly as the binding number
            // to match the sparse ATTRIB_BINDING / POSITION_SSBO_BINDING macros that
            // CompositorAssembler hard-codes into the GLSL source (e.g. Normal=0, TexCoord0=3, Position=8).
            for (size_t idx = 0; idx < std::size(p.ssbo_descriptor_map); ++idx)
            {
                auto *d = p.ssbo_descriptor_map[idx];
                if (d) { d->set = set; d->binding = (int)idx; }
            }
        }
        else
        {
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
        }

        ++set;
    }
}
}}//namespace hgl::graph
