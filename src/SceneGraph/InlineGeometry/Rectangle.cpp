#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    Geometry *CreateRectangle(GeometryCreater *pc,const RectangleCreateInfo *rci)
    {
        if(!pc)return(nullptr);

        if(!pc->Init("Rectangle",4,0))
            return(nullptr);

        VABMap2f vertex(pc->GetVABMap(VAN::Position));

        if(!vertex.IsValid())
            return(nullptr);

        vertex->WriteRectFan(rci->scope);

        return pc->Create();
    }

    Geometry *CreateGBufferCompositionRectangle(GeometryCreater *pc)
    {
        RectangleCreateInfo rci;

        rci.scope.Set(-1,-1,2,2);

        return CreateRectangle(pc,&rci);
    }
} // namespace
