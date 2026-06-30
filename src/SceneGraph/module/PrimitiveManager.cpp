#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/log/Logger.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/module/MaterialBindingInstanceInternalAccess.h>
#include<cstdio>

namespace hgl::graph{

GRAPH_MODULE_CONSTRUCT(PrimitiveManager)
{
}

Primitive *PrimitiveManager::CreatePrimitive(Geometry *r, MaterialBindingInstance *mi, GraphicsPipelinePreRaster *p)
{
    if(!p||!mi||!r)
        return(nullptr);

    Primitive *ri=hgl::graph::DirectCreatePrimitive(r,mi,p);

    if(ri)
        Add(ri);

    return ri;
}

Primitive *PrimitiveManager::CreatePrimitive(GeometryCreater *pc, MaterialBindingInstance *mi, GraphicsPipelinePreRaster *p)
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

Primitive *PrimitiveManager::CreatePrimitive(Geometry *r, MaterialBindingInstance *mi, const VIL *vil)
{
    if(!mi||!r)
        return(nullptr);

    auto *material = MaterialBindingInstanceInternalAccess::GetShaderMaterialProgram(mi);
    GLogError(
        "[PrimitiveManager] CreatePrimitive(geom,mi,vil): geom=%p mi=%p vil=%p material=%p material_prim=%u\n",
        r,
        mi,
        vil,
        material,
        material ? static_cast<unsigned>(material->GetPrimitiveType()) : 0u);

    Primitive *ri=hgl::graph::DirectCreatePrimitive(r,mi,nullptr,vil);

    if(ri)
        Add(ri);

    return ri;
}

Primitive *PrimitiveManager::CreatePrimitive(GeometryCreater *pc, MaterialBindingInstance *mi, const VIL *vil)
{
    if(!mi||!pc)
        return(nullptr);

    Geometry *geometry=pc->Create();

    if(!geometry)
        return(nullptr);

    Primitive *ri=hgl::graph::DirectCreatePrimitive(geometry,mi,nullptr,vil);

    if(ri)
    {
        Add(ri);
        return ri;
    }

    delete geometry;
    return(nullptr);
}

}//namespace hgl::graph
