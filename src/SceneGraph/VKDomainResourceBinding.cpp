#include<hgl/vk/VKDomainResourceBinding.h>
#include<hgl/vk/VKMaterialParameters.h>

namespace hgl::graph
{

DomainResourceBinding::DomainResourceBinding(ResourceDomain *d, ShaderMaterialProgram *m, MaterialParameters *mp)
    : domain(d), material(m), mp_per_material(mp)
{}

DomainResourceBinding::~DomainResourceBinding()
{
    delete mp_per_material;
    mp_per_material = nullptr;
}

bool DomainResourceBinding::BindTexture(const mtl::SamplerSlot slot, Texture *tex)
{
    if (!mp_per_material) return false;
    return mp_per_material->BindTexture(slot, tex);
}

bool DomainResourceBinding::BindTextureSampler(const mtl::SamplerSlot slot,
                                               Texture *tex, Sampler *sampler)
{
    if (!mp_per_material) return false;
    return mp_per_material->BindTextureSampler(slot, tex, sampler);
}

void DomainResourceBinding::Update()
{
    if (mp_per_material) mp_per_material->Update();
}

} // namespace hgl::graph
