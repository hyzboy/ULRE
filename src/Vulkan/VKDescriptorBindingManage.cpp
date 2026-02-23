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

bool DescriptorBinding::Bind(Material *mtl)
{
    if(!mtl)
        return(false);

    MaterialParameters *mp=mtl->GetMP(set_type);

    if(!mp)
        return(false);

    const BindingMapArray &bma=mp->GetBindingMap();

//    const BindingMap &ssbo_bm=bma[VK_DESCRIPTOR_TYPE_STORAGE_BUFFER];
//    const BindingMap &ssbo_dynamic_bm=bma[VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC];

//    const BindingMap &texture_bm=bma[VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER];

    BindUBO(mp,bma[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER],false);
    BindUBO(mp,bma[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC],true);

    mp->Update();
    return(true);
}
}//namespace hgl::graph
