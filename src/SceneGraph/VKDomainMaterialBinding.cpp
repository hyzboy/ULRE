#include<hgl/vk/VKDomainMaterialBinding.h>
#include<hgl/vk/VKMaterialParameters.h>
#include<hgl/type/MemoryUtil.h>

namespace hgl::graph
{

DomainMaterialBinding::DomainMaterialBinding(ResourceDomain *d, Material *m,
                                             MaterialParameters *mp[DESCRIPTOR_SET_TYPE_COUNT])
    : domain(d), material(m)
{
    for(size_t i = 0; i < DESCRIPTOR_SET_TYPE_COUNT; ++i)
        mp_array[i] = mp[i];
}

DomainMaterialBinding::~DomainMaterialBinding()
{
    for(auto &mp : mp_array)
    {
        delete mp;
        mp = nullptr;
    }
}

bool DomainMaterialBinding::BindTexture(const DescriptorSetType &type, mtl::SamplerName::SamplerSlot slot, Texture *tex)
{
    MaterialParameters *mp = GetMP(type);
    if(!mp) return false;
    const AnsiString name = mtl::SamplerName::ToDescriptorName(slot);
    return mp->BindTexture(name, tex);
}

bool DomainMaterialBinding::BindTextureSampler(const DescriptorSetType &type, mtl::SamplerName::SamplerSlot slot,
                                               Texture *tex, Sampler *sampler)
{
    MaterialParameters *mp = GetMP(type);
    if(!mp) return false;
    const AnsiString name = mtl::SamplerName::ToDescriptorName(slot);
    return mp->BindTextureSampler(name, tex, sampler);
}

bool DomainMaterialBinding::BindUBO(const DescriptorSetType &type, const AnsiString &name,
                                    const IGPUBuffer *gpu, bool dynamic)
{
    MaterialParameters *mp = GetMP(type);
    if(!mp) return false;
    return mp->BindUBO(name, gpu, dynamic);
}

bool DomainMaterialBinding::BindSSBO(const DescriptorSetType &type, const AnsiString &name,
                                     const IGPUBuffer *gpu, bool dynamic)
{
    MaterialParameters *mp = GetMP(type);
    if(!mp) return false;
    return mp->BindSSBO(name, gpu, dynamic);
}

void DomainMaterialBinding::Update()
{
    for(auto *mp : mp_array)
    {
        if(mp) mp->Update();
    }
}

} // namespace hgl::graph
