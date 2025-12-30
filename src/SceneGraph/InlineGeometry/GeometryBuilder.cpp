#include<hgl/graph/geo/GeometryBuilder.h>

namespace hgl::graph::inline_geometry
{
    GeometryBuilder::GeometryBuilder(GeometryCreater *pc)
        : creater(pc)
        , vp(nullptr)
        , np(nullptr)
        , tp(nullptr)
        , tcp(nullptr)
    {
        if(!pc)
            return;

        // 初始化VAB映射
        VABMapFloat vertex   (pc->GetVABMap(VAN::Position), VF_V3F);
        VABMapFloat normal   (pc->GetVABMap(VAN::Normal),   VF_V3F);
        VABMapFloat tangent  (pc->GetVABMap(VAN::Tangent),  VF_V3F);
        VABMapFloat tex_coord(pc->GetVABMap(VAN::TexCoord), VF_V2F);

        vp  = vertex;
        np  = normal;
        tp  = tangent;
        tcp = tex_coord;
    }
}
