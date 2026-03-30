#pragma once

#include<hgl/graph/module/GraphModule.h>
#include<hgl/type/UnorderedMap.h>
#include<iostream>

namespace hgl::graph{

GRAPH_MODULE_CLASS(RenderPassManager)
{
    UnorderedMap<AnsiString,RenderPass *> RenderPassList;

private:

    RenderPassManager(GraphicsContext *);
    ~RenderPassManager()
    {
        Release();
    }

    friend class GraphModuleManager;

public:

    RenderPass *    AcquireRenderPass(const RenderbufferInfo *);

    void Release() override;
};//class RenderPassManager

}//namespace hgl::graph
