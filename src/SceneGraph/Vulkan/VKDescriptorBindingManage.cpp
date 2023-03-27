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

    {
        const auto *dp=ubo_map.GetDataList();

        for(uint i=0;i<ubo_map.GetCount();i++)
        {
            mp->BindUBO((*dp)->left,(*dp)->right);

            ++dp;
        }
    }

    {
        const auto *dp=ssbo_map.GetDataList();

        for(uint i=0;i<ssbo_map.GetCount();i++)
        {
            mp->BindSSBO((*dp)->left,(*dp)->right);

            ++dp;
        }
    }

    //{
    //    const auto *dp=texture_map.GetDataList();

    //    for(uint i=0;i<texture_map.GetCount();i++)
    //    {
    //        mp->BindImageSampler((*dp)->left,(*dp)->right);

    //        ++dp;
    //    }
    //}

    mp->Update();
    return(true);
}
VK_NAMESPACE_END
