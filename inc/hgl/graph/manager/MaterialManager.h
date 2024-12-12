#pragma once

#include<hgl/graph/module/GraphModule.h>

VK_NAMESPACE_BEGIN

class MaterialManager:public GraphModule
{


public:

    GRAPH_MODULE_CONSTRUCT(MaterialManager)
    virtual ~MaterialManager();

};//class MaterialManager:public GraphModule

VK_NAMESPACE_END