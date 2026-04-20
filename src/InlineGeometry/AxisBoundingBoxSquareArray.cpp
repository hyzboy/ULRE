#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    Geometry *CreateAxis(GeometryCreater *pc,const AxisCreateInfo *aci)
    {
        if(!pc||!aci)return(nullptr);

        if(!pc)return(nullptr);

        if(!pc->Init("Axis",6,0))
            return(nullptr);

        GeometryBuilder builder(pc);

        if(!builder.IsValid() || !builder.HasColors())
            return(nullptr);

        const float s=aci->size;

        builder.WriteVertex(0,0,0);builder.WriteColor(aci->color[0].x, aci->color[0].y, aci->color[0].z, aci->color[0].w);
        builder.WriteVertex(s,0,0);builder.WriteColor(aci->color[0].x, aci->color[0].y, aci->color[0].z, aci->color[0].w);
        builder.WriteVertex(0,0,0);builder.WriteColor(aci->color[1].x, aci->color[1].y, aci->color[1].z, aci->color[1].w);
        builder.WriteVertex(0,s,0);builder.WriteColor(aci->color[1].x, aci->color[1].y, aci->color[1].z, aci->color[1].w);
        builder.WriteVertex(0,0,0);builder.WriteColor(aci->color[2].x, aci->color[2].y, aci->color[2].z, aci->color[2].w);
        builder.WriteVertex(0,0,s);builder.WriteColor(aci->color[2].x, aci->color[2].y, aci->color[2].z, aci->color[2].w);

        return pc->CreateWithAABB(
            math::Vector3f(0,0,0),
            math::Vector3f(s,s,s));
    }

    Geometry *CreateBoundingBox(GeometryCreater *pc,const BoundingBoxCreateInfo *cci)
    {
        const float points[]={  -0.5,-0.5, 0.5,     0.5,-0.5,0.5,   0.5,-0.5,-0.5,  -0.5,-0.5,-0.5,
                                -0.5, 0.5, 0.5,     0.5, 0.5,0.5,   0.5, 0.5,-0.5,  -0.5, 0.5,-0.5};

        const uint16 indices[]={
            0,1,    1,2,    2,3,    3,0,
            4,5,    5,6,    6,7,    7,4,
            0,4,    1,5,    2,6,    3,7
        };

        if(!pc)return(nullptr);

        if(!pc->Init("BoundingBox",8,24,IndexType::U16))
            return(nullptr);

        if(!pc->WriteVAB(VAN::Position,VF_V3F,points))
            return(nullptr);

        if(cci->color_type!=BoundingBoxCreateInfo::ColorType::NoColor)
        {
            RANGE_CHECK_RETURN_NULLPTR(cci->color_type);

            GeometryBuilder builder(pc);

            if(builder.HasColors())
            {
                if(cci->color_type==BoundingBoxCreateInfo::ColorType::SameColor)
                    for(int i=0;i<8;i++)
                        builder.WriteColor(cci->color[0].x, cci->color[0].y, cci->color[0].z, cci->color[0].w);
                else
                if(cci->color_type==BoundingBoxCreateInfo::ColorType::VertexColor)
                    for(int i=0;i<8;i++)
                        builder.WriteColor(cci->color[i].x, cci->color[i].y, cci->color[i].z, cci->color[i].w);
            }
        }

        pc->WriteIBO<uint16>(indices);

        return pc->CreateWithAABB(
            math::Vector3f(-0.5,-0.5,-0.5),
            math::Vector3f(0.5,0.5,0.5));
    }

    Geometry *CreateSqaureArray(GeometryCreater *pc,const uint row,const uint col)
    {
        if(!pc)return(nullptr);
        if(row==0||col==0)return(nullptr);
        if (row>=255||col>=255)return(nullptr); //顶点坐标使用 uint8

        const uint numberVertices=(row+1)*(col+1);
        const uint numberIndices=row*col*6;

        if(!pc->Init("SquareArray",numberVertices,numberIndices,IndexType::U16))
            return(nullptr);

        {
            auto vertex = pc->GetBufferAccessor<BufferAccessor2u8>(VAN::Position);  //顶点坐标使用 uint8

            if(!vertex.IsValid())
                return(nullptr);

            for(uint i=0;i<=row;i++)
                for(uint j=0;j<=col;j++)
                    vertex->Write(j,i);
        }

        {
            auto ib_map = pc->GetIndexAccessor<uint16>();

            uint16 *tp=ib_map;

            for(uint i=0;i<row;i++)
                for(uint j=0;j<col;j++)
                {
                    const uint16 v0=(i  )*(col+1)+(j  );
                    const uint16 v1=(i  )*(col+1)+(j+1);
                    const uint16 v2=(i+1)*(col+1)+(j+1);
                    const uint16 v3=(i+1)*(col+1)+(j  );
                    *tp=v0;++tp;
                    *tp=v2;++tp;
                    *tp=v1;++tp;
                    *tp=v0;++tp;
                    *tp=v3;++tp;
                    *tp=v2;++tp;
                }
        }

        return pc->CreateWithAABB(
            math::Vector3f(0,0,0),
            math::Vector3f(col,row,0));
    }
} // namespace
