#pragma once

#include<hgl/graph/module/GraphModule.h>
#include<hgl/graph/VKGeometry.h>
#include<hgl/type/ObjectManager.h>

VK_NAMESPACE_BEGIN

class RenderFramework;
class Geometry;

using GeometryID=int;

GRAPH_MODULE_CLASS(GeometryManager)
{
private:

    AutoIdObjectManager<GeometryID,Geometry> rm_geometry;              ///<图元合集

    GeometryManager(RenderFramework *);
    virtual ~GeometryManager()=default;

    friend class GraphModuleManager;

public:

    GeometryID  Add         (Geometry *        p   ){return rm_geometry.Add(p);}
    Geometry *  GetGeometry (const GeometryID &id  ){return rm_geometry.Get(id);}
    void        Release     (Geometry *        p   ){rm_geometry.Release(p);}
};//class GeometryManager

VK_NAMESPACE_END
