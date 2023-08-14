#pragma once
#include<hgl/graph/MaterialRenderList.h>

VK_NAMESPACE_BEGIN
class MaterialRenderMap:public ObjectMap<Material *,MaterialRenderList>
{
public:

    MaterialRenderMap()=default;
    virtual ~MaterialRenderMap()=default;

    void Begin()
    {
        for(auto *it:data_list)
            it->value->Clear();
    }

    void End()
    {
        for(auto *it:data_list)
            it->value->End();
    }

    void Render(RenderCmdBuffer *rcb)
    {
        if(!rcb)return;

        for(auto *it:data_list)
            it->value->Render(rcb);
    }
};//class MaterialRenderMap
VK_NAMESPACE_END
