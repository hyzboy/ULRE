#include<hgl/graph/VKDescriptorBindingManage.h>
#include<hgl/graph/VKMaterialParameters.h>
#include<hgl/graph/VKMaterialInstance.h>

VK_NAMESPACE_BEGIN
void DescriptorBinding::BindUBO(MaterialParameters *mp,const BindingMap &binding_map)
{
    DeviceBuffer* buf = nullptr;

    if (binding_map.GetCount() > 0)
    {
        const auto* dp = binding_map.GetDataList();
        const uint count = binding_map.GetCount();

        for (uint i = 0; i < count; i++)
        {
            buf = GetUBO((*dp)->key);

            if (buf)
                mp->BindUBO((*dp)->value, buf, false);

            ++dp;
        }
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

    BindUBO(mp,bma[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER]);
    BindUBO(mp,bma[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC]);

    mp->Update();
    return(true);
}
VK_NAMESPACE_END
