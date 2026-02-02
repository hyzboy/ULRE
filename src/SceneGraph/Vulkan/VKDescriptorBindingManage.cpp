#include<hgl/graph/VKDescriptorBindingManage.h>
#include<hgl/graph/VKMaterialParameters.h>
#include<hgl/graph/VKMaterialInstance.h>

VK_NAMESPACE_BEGIN
void DescriptorBinding::BindUBO(MaterialParameters *mp,const BindingMap &binding_map,bool dynamic)
{
    if (binding_map.GetCount() <= 0)return;

    DeviceBuffer* buf = nullptr;

    for(const auto& [name, binding] : binding_map)
    {
        buf=GetUBO(name);

        if(buf)
            mp->BindUBO(binding,buf,dynamic);
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
VK_NAMESPACE_END
