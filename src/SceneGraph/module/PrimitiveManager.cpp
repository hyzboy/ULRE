#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/geo/VKGeometry.h>
#include<hgl/vk/VKVertexAttribBuffer.h>

namespace
{
    static uint32_t ComputeVILHashFromGeometry(const hgl::graph::Geometry *geo)
    {
        if (!geo)
            return 0;

        // Keep hash logic aligned with runtime resolve path (FNV-1a over attrib/format stream).
        uint32_t h = 2166136261u;

        const int count = geo->GetVABCount();
        for (int i = 0; i < count; ++i)
        {
            const auto *vab = geo->GetVAB(i);
            if (!vab)
                continue;

            const uint32_t packed = (static_cast<uint32_t>(i) << 16)
                                  ^ static_cast<uint32_t>(vab->GetFormat());

            h ^= packed;
            h *= 16777619u;
        }

        return h;
    }
}

namespace hgl::graph{

GRAPH_MODULE_CONSTRUCT(PrimitiveManager)
{
}

Primitive *PrimitiveManager::CreatePrimitive(Geometry *r, MaterialInstance *mi, GraphicsPipelinePreRaster *p)
{
    if(!p||!mi||!r)
        return(nullptr);

    Primitive *ri=hgl::graph::DirectCreatePrimitive(r,mi,p);

    if(ri)
        Add(ri);

    return ri;
}

Primitive *PrimitiveManager::CreatePrimitive(GeometryCreater *pc, MaterialInstance *mi, GraphicsPipelinePreRaster *p)
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

Primitive *PrimitiveManager::CreatePrimitive(Geometry *r, MaterialInstance *mi)
{
    if(!mi||!r)
        return(nullptr);

    Primitive *ri=hgl::graph::DirectCreatePrimitive(r,mi,nullptr);

    if(ri)
        Add(ri);

    return ri;
}

Primitive *PrimitiveManager::CreatePrimitive(GeometryCreater *pc, MaterialInstance *mi)
{
    if(!mi||!pc)
        return(nullptr);

    Geometry *geometry=pc->Create();

    if(!geometry)
        return(nullptr);

    Primitive *ri=hgl::graph::DirectCreatePrimitive(geometry,mi,nullptr);

    if(ri)
    {
        Add(ri);
        return ri;
    }

    delete geometry;
    return(nullptr);
}

Primitive *PrimitiveManager::CreatePrimitive(Geometry *r, SemanticMaterialId semantic_id)
{
    if(!r || semantic_id == 0)
        return nullptr;

    const uint32_t vil_hash = ComputeVILHashFromGeometry(r);
    Primitive *ri = hgl::graph::DirectCreatePrimitive(r, semantic_id, vil_hash);

    if(ri)
        Add(ri);

    return ri;
}

Primitive *PrimitiveManager::CreatePrimitive(GeometryCreater *pc, SemanticMaterialId semantic_id)
{
    if(!pc || semantic_id == 0)
        return nullptr;

    Geometry *geometry = pc->Create();

    if(!geometry)
        return nullptr;

    const uint32_t vil_hash = ComputeVILHashFromGeometry(geometry);
    Primitive *ri = hgl::graph::DirectCreatePrimitive(geometry, semantic_id, vil_hash);

    if(ri)
    {
        Add(ri);
        return ri;
    }

    delete geometry;
    return nullptr;
}

}//namespace hgl::graph
