#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/geo/GeometryCreater.h>

namespace hgl::graph{

GRAPH_MODULE_CONSTRUCT(PrimitiveManager)
{
}

Primitive *PrimitiveManager::CreatePrimitive(Geometry *r, MaterialInstance *mi, GraphicsPipeline *p)
{
    if(!p||!mi||!r)
        return(nullptr);

    Primitive *ri=hgl::graph::DirectCreatePrimitive(r,mi,p);

    if(ri)
        Add(ri);

    return ri;
}

Primitive *PrimitiveManager::CreatePrimitive(GeometryCreater *pc, MaterialInstance *mi, GraphicsPipeline *p)
{
    if(!p||!mi||!pc)
        return(nullptr);

    Geometry *geometry=pc->Create();

    if(!geometry)
        return(nullptr);

    Primitive *ri=hgl::graph::DirectCreatePrimitive(geometry,mi,p);

    if(ri)
    {
        // Add geometry ownership remains responsibility of caller in many places; keep behavior similar to previous
        Add(ri);
        return ri;
    }

    delete geometry;
    return(nullptr);
}

}//namespace hgl::graph
