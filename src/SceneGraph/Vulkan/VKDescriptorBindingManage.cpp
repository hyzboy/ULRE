#include<hgl/graph/VKDescriptorBindingManage.h>
#include<hgl/graph/VKMaterialParameters.h>

VK_NAMESPACE_BEGIN
bool DescriptorBinding::Bind(MaterialParameters *mp)
{
    if(!mp)return(false);

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
}
VK_NAMESPACE_END
