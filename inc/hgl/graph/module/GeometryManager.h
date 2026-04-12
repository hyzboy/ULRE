#pragma once

#include<hgl/graph/module/GraphModule.h>
#include<hgl/graph/geo/VKGeometry.h>
#include<hgl/type/ObjectManager.h>

namespace hgl::graph{

class Geometry;

using GeometryID=int;

GRAPH_MODULE_CLASS(GeometryManager)
{
private:

    SharedObjectManager<GeometryID,Geometry> rm_geometry;              ///<图元合集

    GeometryManager(GraphicsContext *);
    virtual ~GeometryManager()=default;

    friend class GraphModuleManager;

public:

    GeometryID               Add        (Geometry *p)          {return rm_geometry.Add(p);}
    std::weak_ptr<Geometry>  GetGeometry(const GeometryID &id) {return rm_geometry.Get(id);}
    void                     Release    (Geometry *p)           {rm_geometry.Release(p);}
    void                     Release    (const GeometryID &id)  {rm_geometry.Release(id);}

    void Release() override
    {
        if (rm_geometry.GetCount() > 0)
            rm_geometry.Clear();
    }
};//class GeometryManager

}//namespace hgl::graph
