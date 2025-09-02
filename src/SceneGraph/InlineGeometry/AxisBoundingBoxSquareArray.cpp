#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    Primitive *CreateAxis(PrimitiveCreater *pc,const AxisCreateInfo *aci)
    {
        if(!pc||!aci)return(nullptr);

        if(!pc)return(nullptr);

        if(!pc->Init("Axis",6,0))
            return(nullptr);

        VABMap3f vertex(pc->GetVABMap(VAN::Position));
        VABMap4f color(pc->GetVABMap(VAN::Color));

        if(!vertex.IsValid()||!color.IsValid())
            return(nullptr);

        const float s=aci->size;

        vertex->Write(0,0,0);color->Write(aci->color[0]);
        vertex->Write(s,0,0);color->Write(aci->color[0]);
        vertex->Write(0,0,0);color->Write(aci->color[1]);
        vertex->Write(0,s,0);color->Write(aci->color[1]);
        vertex->Write(0,0,0);color->Write(aci->color[2]);
        vertex->Write(0,0,s);color->Write(aci->color[2]);

        Primitive *p=pc->Create();

        p->SetBoundingBox(Vector3f(0,0,0),Vector3f(s,s,s));

        return p;
    }

    Primitive *CreateBoundingBox(PrimitiveCreater *pc,const BoundingBoxCreateInfo *cci)
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

            VABMap4f color(pc->GetVABMap(VAN::Color));

            if(color.IsValid())
            {
                if(cci->color_type==BoundingBoxCreateInfo::ColorType::SameColor)
                    color->RepeatWrite(cci->color[0],8);
                else
                if(cci->color_type==BoundingBoxCreateInfo::ColorType::VertexColor)
                    color->Write(cci->color,8);
            }
        }

        pc->WriteIBO<uint16>(indices);

        Primitive *p=pc->Create();

        p->SetBoundingBox(  Vector3f(-0.5,-0.5,-0.5),
                            Vector3f( 0.5, 0.5, 0.5));

        return p;
    }

    Primitive *CreateSqaureArray(PrimitiveCreater *pc,const uint row,const uint col)
    {
        if(!pc)return(nullptr);
        if(row==0||col==0)return(nullptr);
        if (row>=255||col>=255)return(nullptr); //顶点坐标使用 uint8

        const uint numberVertices=(row+1)*(col+1);
        const uint numberIndices=row*col*6;

        if(!pc->Init("SquareArray",numberVertices,numberIndices,IndexType::U16))
            return(nullptr);

        {
            VABMap2u8 vertex(pc->GetVABMap(VAN::Position),VF_V2U8);       //顶点坐标使用 uint8

            if(!vertex.IsValid())
                return(nullptr);

            for(uint i=0;i<=row;i++)
                for(uint j=0;j<=col;j++)
                    vertex->Write(j,i);
        }

        {
            IBTypeMap<uint16> ib_map(pc->GetIBMap());

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

        Primitive *p=pc->Create();
        {
            AABB aabb;
            aabb.SetMinMax(Vector3f(0,0,0),Vector3f(col,row,0));
            p->SetBoundingBox(aabb);
        }

        return p;
    }
} // namespace
