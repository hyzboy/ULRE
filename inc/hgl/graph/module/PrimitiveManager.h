#pragma once

#include<hgl/graph/module/GraphModule.h>
#include<hgl/graph/VKPrimitive.h>
#include<hgl/type/ObjectManage.h>

VK_NAMESPACE_BEGIN

class RenderFramework;
class Primitive;

using PrimitiveID=int;

GRAPH_MODULE_CLASS(PrimitiveManager)
{
private:

    IDObjectManage<PrimitiveID,Primitive> rm_primitives;              ///<图元合集

    PrimitiveManager(RenderFramework *);
    virtual ~PrimitiveManager()=default;

    friend class GraphModuleManager;

public:

    PrimitiveID Add         (Primitive *        p   ){return rm_primitives.Add(p);}
    Primitive * GetPrimitive(const PrimitiveID &id  ){return rm_primitives.Get(id);}
    void        Release     (Primitive *        p   ){rm_primitives.Release(p);}
};//class PrimitiveManager

VK_NAMESPACE_END
