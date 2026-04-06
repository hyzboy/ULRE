#include<hgl/vk/VKDomainMaterialBinding.h>
#include<hgl/vk/VKMaterialParameters.h>

namespace hgl::graph
{

DomainMaterialBinding::DomainMaterialBinding(MaterialResourceDomain *d, MaterialTemplate *m, MaterialParameters *mp)
    : domain(d), material(m), mp_per_material(mp)
{}

DomainMaterialBinding::~DomainMaterialBinding()
{
    delete mp_per_material;
    mp_per_material = nullptr;
}

bool DomainMaterialBinding::BindTexture(const mtl::SamplerSlot slot, Texture *tex)
{
    if (!mp_per_material) return false;
    return mp_per_material->BindTexture(slot, tex);
}

bool DomainMaterialBinding::BindTextureSampler(const mtl::SamplerSlot slot,
                                               Texture *tex, Sampler *sampler)
{
    if (!mp_per_material) return false;
    return mp_per_material->BindTextureSampler(slot, tex, sampler);
}

void DomainMaterialBinding::Update()
{
    if (mp_per_material) mp_per_material->Update();
}

} // namespace hgl::graph
