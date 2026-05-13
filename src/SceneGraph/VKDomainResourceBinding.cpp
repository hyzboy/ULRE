#include<hgl/vk/VKDomainResourceBinding.h>
#include<hgl/vk/VKMaterialParameters.h>
#include<hgl/log/Log.h>

namespace hgl::graph
{

DomainResourceBinding::DomainResourceBinding(ResourceDomain *d, ShaderMaterialProgram *m,
                                             MaterialParameters *mp_mat, MaterialParameters *mp_obj)
    : domain(d), material(m), mp_per_material(mp_mat), mp_per_object(mp_obj)
{}

DomainResourceBinding::~DomainResourceBinding()
{
    delete mp_per_material;
    mp_per_material = nullptr;

    delete mp_per_object;
    mp_per_object = nullptr;
}

bool DomainResourceBinding::BindTexture(const mtl::SamplerSlot slot, Texture *tex)
{
    if (!mp_per_material) return false;
    return mp_per_material->BindTexture(slot, tex);
}

bool DomainResourceBinding::BindResourceSampler(const mtl::SamplerSlot slot,
                                               Texture *tex, Sampler *sampler)
{
    if (!mp_per_material) return false;
    GLogInfo("[TBV][DomainResourceBinding] BindResourceSampler: binding=%p domain=%p material=%p(%s) mp=%p ds=%p slot=%u tex=%p sampler=%p",
            static_cast<void *>(this),
            static_cast<void *>(domain),
            static_cast<void *>(material),
            material ? material->GetName().c_str() : "<null>",
            static_cast<void *>(mp_per_material),
            (void *)mp_per_material->GetVkDescriptorSet(),
            static_cast<unsigned>(slot),
            static_cast<void *>(tex),
            static_cast<void *>(sampler));
    return mp_per_material->BindResourceSampler(slot, tex, sampler);
}

void DomainResourceBinding::Update()
{
    if (mp_per_material)
    {
        GLogInfo("[TBV][DomainResourceBinding] Update: binding=%p domain=%p material=%p(%s) mp=%p ds=%p po=%p po_ds=%p",
                static_cast<void *>(this),
                static_cast<void *>(domain),
                static_cast<void *>(material),
                material ? material->GetName().c_str() : "<null>",
                static_cast<void *>(mp_per_material),
                (void *)mp_per_material->GetVkDescriptorSet(),
                static_cast<void *>(mp_per_object),
                mp_per_object ? (void *)mp_per_object->GetVkDescriptorSet() : nullptr);
        mp_per_material->Update();
    }
}

} // namespace hgl::graph
