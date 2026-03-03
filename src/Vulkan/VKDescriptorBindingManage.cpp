#include<hgl/vk/VKDescriptorBindingManage.h>
#include<hgl/vk/VKMaterialParameters.h>
#include<hgl/vk/VKMaterialInstance.h>

namespace hgl::graph{
void DescriptorBinding::BindUBO(MaterialParameters *mp,const BindingMap &binding_map,bool dynamic)
{
    if (binding_map.GetCount() <= 0)return;

    const IGPUBuffer* gpu = nullptr;

    for(const auto& [name, binding] : binding_map)
    {
        gpu=GetUBO(name);

        if(gpu)
            mp->BindUBO(binding,gpu,dynamic);
    }
}

void DescriptorBinding::BindSSBO(MaterialParameters *mp,const BindingMap &binding_map,bool dynamic)
{
    if (binding_map.GetCount() <= 0)return;

    const IGPUBuffer* gpu = nullptr;

    for(const auto& [name, binding] : binding_map)
    {
        gpu=GetSSBO(name);

        if(gpu)
            mp->BindSSBO(binding,gpu,dynamic);
    }
}

void DescriptorBinding::BindTexture(MaterialParameters *mp,const BindingMap &binding_map)
{
    if (binding_map.GetCount() <= 0)return;

    Texture *tex = nullptr;

    for(const auto& [name, binding] : binding_map)
    {
        tex=GetTexture(name);

        if(tex)
            mp->BindTexture(binding,tex);
    }
}

void DescriptorBinding::BindTextureSampler(MaterialParameters *mp,const BindingMap &binding_map)
{
    if (binding_map.GetCount() <= 0)return;

    const TextureSamplerBinding *tsb = nullptr;

    for(const auto& [name, binding] : binding_map)
    {
        tsb=GetTextureSampler(name);

        if(tsb&&tsb->texture&&tsb->sampler)
            mp->BindTextureSampler(binding,tsb->texture,tsb->sampler);
    }
}

bool DescriptorBinding::Bind(Material *mtl)
{
    if(!mtl)
        return(false);

    MaterialParameters *mp=mtl->GetMP(set_type);

    if(mp)
    {
        const BindingMapArray &bma=mp->GetBindingMap();

        BindUBO(mp,bma[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER],false);
        BindUBO(mp,bma[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC],true);
        BindSSBO(mp,bma[VK_DESCRIPTOR_TYPE_STORAGE_BUFFER],false);
        BindSSBO(mp,bma[VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC],true);
        BindTexture(mp,bma[VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE]);
        BindTextureSampler(mp,bma[VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER]);

        mp->Update();
        return(true);
    }

    bool has_bound=false;

    for(size_t i=0;i<DESCRIPTOR_SET_TYPE_COUNT;i++)
    {
        MaterialParameters *fallback_mp=mtl->GetMP(static_cast<DescriptorSetType>(i));

        if(!fallback_mp)
            continue;

        const BindingMapArray &bma=fallback_mp->GetBindingMap();

        BindUBO(fallback_mp,bma[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER],false);
        BindUBO(fallback_mp,bma[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC],true);
        BindSSBO(fallback_mp,bma[VK_DESCRIPTOR_TYPE_STORAGE_BUFFER],false);
        BindSSBO(fallback_mp,bma[VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC],true);
        BindTexture(fallback_mp,bma[VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE]);
        BindTextureSampler(fallback_mp,bma[VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER]);

        fallback_mp->Update();
        has_bound=true;
    }

    return has_bound;
}
}//namespace hgl::graph
