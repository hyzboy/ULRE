#include<hgl/graph/VKDescriptorBindingManage.h>
#include<hgl/graph/VKMaterialParameters.h>
#include<hgl/graph/VKMaterialInstance.h>

VK_NAMESPACE_BEGIN
bool DescriptorBinding::Bind(MaterialInstance *mi)
{
    if(!mi)
        return(false);

    MaterialParameters *mp=mi->GetMP(set_type);

    if(!mp)
        return(false);

    const BindingMapArray &bma=mp->GetBindingMap();

    const BindingMap &ubo_bm=bma[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER];
    const BindingMap &ubo_dynamic_bm=bma[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC];

//    const BindingMap &ssbo_bm=bma[VK_DESCRIPTOR_TYPE_STORAGE_BUFFER];
//    const BindingMap &ssbo_dynamic_bm=bma[VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC];

//    const BindingMap &texture_bm=bma[VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER];

    DeviceBuffer *buf=nullptr;

    if(ubo_bm.GetCount()>0)
    {
        const auto *dp=ubo_bm.GetDataList();
        const uint count=ubo_bm.GetCount();

        for(uint i=0;i<count;i++)
        {
            buf=GetUBO((*dp)->left);

            if(buf)
                mp->BindUBO((*dp)->right,buf,false);

            ++dp;
        }
    }

    if(ubo_dynamic_bm.GetCount()>0)
    {
        const auto *dp=ubo_dynamic_bm.GetDataList();
        const uint count=ubo_dynamic_bm.GetCount();

        for(uint i=0;i<count;i++)
        {
            buf=GetUBO((*dp)->left);

            if(buf)
                mp->BindUBO((*dp)->right,buf,true);

            ++dp;
        }
    }

    mp->Update();
    return(true);
}
VK_NAMESPACE_END
