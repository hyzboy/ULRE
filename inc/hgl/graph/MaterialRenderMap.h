#pragma once
#include<hgl/graph/MaterialRenderList.h>

VK_NAMESPACE_BEGIN
class MaterialRenderMap:public ObjectMap<RenderPipelineIndex,MaterialRenderList>
{
public:

    MaterialRenderMap()=default;
    virtual ~MaterialRenderMap()=default;

    void Begin(CameraInfo *ci)
    {
        for(auto *it:data_list)
        {
            it->value->SetCameraInfo(ci);
            it->value->Clear();
        }
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

    void UpdateLocalToWorld()
    {
        for(auto *it:data_list)
            it->value->UpdateLocalToWorld();
    }
};//class MaterialRenderMap
VK_NAMESPACE_END
