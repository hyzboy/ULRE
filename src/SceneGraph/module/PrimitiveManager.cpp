#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/RenderFramework.h>

VK_NAMESPACE_BEGIN

GRAPH_MODULE_CONSTRUCT(PrimitiveManager)
{
}

Primitive *PrimitiveManager::CreatePrimitive(Geometry *r, MaterialInstance *mi, Pipeline *p)
{
    if(!p||!mi||!r)
        return(nullptr);

    Primitive *ri=VK_NAMESPACE::DirectCreatePrimitive(r,mi,p);

    if(ri)
        Add(ri);

    return ri;
}

Primitive *PrimitiveManager::CreatePrimitive(GeometryCreater *pc, MaterialInstance *mi, Pipeline *p)
{
    if(!p||!mi||!pc)
        return(nullptr);

    Geometry *geometry=pc->Create();

    if(!geometry)
        return(nullptr);

    Primitive *ri=VK_NAMESPACE::DirectCreatePrimitive(geometry,mi,p);

    if(ri)
    {
        // Add geometry ownership remains responsibility of caller in many places; keep behavior similar to previous
        Add(ri);
        return ri;
    }

    delete geometry;
    return(nullptr);
}

VK_NAMESPACE_END
