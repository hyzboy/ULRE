#include<hgl/graph/geo/GeometryBuilder.h>
#include<hgl/graph/VKGeometry.h>

namespace hgl::graph::inline_geometry
{
    GeometryBuilder::GeometryBuilder(GeometryCreater *pc)
        : creater(pc)
        , vab_position(nullptr)
        , vab_normal(nullptr)
        , vab_tangent(nullptr)
        , vab_texcoord(nullptr)
        , vp(nullptr)
        , np(nullptr)
        , tp(nullptr)
        , tcp(nullptr)
    {
        if(!pc)
            return;

        // 初始化VAB映射 (keep mapped until GeometryBuilder destruction)
        vab_position = pc->GetVABMap(VAN::Position);
        if (vab_position && vab_position->GetFormat() == VF_V3F)
            vp = static_cast<float *>(vab_position->Map());

        vab_normal = pc->GetVABMap(VAN::Normal);
        if (vab_normal && vab_normal->GetFormat() == VF_V3F)
            np = static_cast<float *>(vab_normal->Map());

        vab_tangent = pc->GetVABMap(VAN::Tangent);
        if (vab_tangent && vab_tangent->GetFormat() == VF_V3F)
            tp = static_cast<float *>(vab_tangent->Map());

        vab_texcoord = pc->GetVABMap(VAN::TexCoord);
        if (vab_texcoord && vab_texcoord->GetFormat() == VF_V2F)
            tcp = static_cast<float *>(vab_texcoord->Map());
    }

    GeometryBuilder::~GeometryBuilder()
    {
        if (vab_position)
            vab_position->Unmap();
        if (vab_normal)
            vab_normal->Unmap();
        if (vab_tangent)
            vab_tangent->Unmap();
        if (vab_texcoord)
            vab_texcoord->Unmap();
    }
}
