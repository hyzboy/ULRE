#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    Primitive *CreateCircle2D(PrimitiveCreater *pc,const CircleCreateInfo *cci)
    {
        if(!pc)return(nullptr);

        uint edge;
        uint vertex_count;

        if(cci->has_center)
        {
            edge=cci->field_count+1;
            vertex_count=cci->field_count+2;
        }
        else
        {
            edge=cci->field_count;
            vertex_count=cci->field_count;
        }

        if(!pc->Init("Circle",vertex_count,0))return(nullptr);

        VABMap2f vertex(pc->GetVABMap(VAN::Position));
        VABMap4f color(pc->GetVABMap(VAN::Color));

        if(!vertex.IsValid())
            return(nullptr);

        if(cci->has_center)
        {
            vertex->Write(cci->center);

            if(color.IsValid())
                color->Write(cci->center_color);
        }

        for(uint i=0;i<edge;i++)
        {
            float ang=float(i)/float(cci->field_count)*360.0f;

            float x=cci->center.x+sin(deg2rad(ang))*cci->radius.x;
            float y=cci->center.y+cos(deg2rad(ang))*cci->radius.y;

            vertex->Write(x,y);
                
            if(color.IsValid())
                color->Write(cci->border_color);
        }

        return pc->Create();
    }

    Primitive *CreateCircle3D(PrimitiveCreater *pc,const CircleCreateInfo *cci)
    {
        if(!pc)return(nullptr);

        uint edge;
        uint vertex_count;

        if(cci->has_center)
        {
            edge=cci->field_count+1;
            vertex_count=cci->field_count+2;
        }
        else
        {
            edge=cci->field_count;
            vertex_count=cci->field_count;
        }

        bool has_index=pc->hasIndex();

        if(!pc->Init("Circle",vertex_count,has_index?vertex_count:0))return(nullptr);

        VABMap3f vertex(pc->GetVABMap(VAN::Position));
        VABMap4f color(pc->GetVABMap(VAN::Color));
        VABMap3f normal(pc->GetVABMap(VAN::Normal));

        if(!vertex.IsValid())
            return(nullptr);

        if(cci->has_center)
        {
            vertex->Write(cci->center.x,cci->center.y,0);

            if(color.IsValid())
                color->Write(cci->center_color);

            if(normal.IsValid())
                normal->Write(AxisVector::Z);
        }

        for(uint i=0;i<edge;i++)
        {
            float ang=float(i)/float(cci->field_count)*360.0f;

            float x=cci->center.x+sin(deg2rad(ang))*cci->radius.x;
            float y=cci->center.y+cos(deg2rad(ang))*cci->radius.y;

            vertex->Write(x,y,0);
                
            if(color.IsValid())
                color->Write(cci->border_color);

            if(normal.IsValid())
                normal->Write(AxisVector::Z);
        }

        if(has_index)
        {
            IBMap *ib_map=pc->GetIBMap();

            if(pc->GetIndexType()==IndexType::U16)WriteIBO<uint16>((uint16 *)(ib_map->Map()),0,vertex_count);else
            if(pc->GetIndexType()==IndexType::U32)WriteIBO<uint32>((uint32 *)(ib_map->Map()),0,vertex_count);else
            if(pc->GetIndexType()==IndexType::U8 )WriteIBO<uint8 >((uint8  *)(ib_map->Map()),0,vertex_count);

            ib_map->Unmap();
        }

        return pc->Create();
    }

    Primitive *CreateCircle3DByIndexTriangles(PrimitiveCreater *pc,const CircleCreateInfo *cci)
    {
        if(!pc)return(nullptr);

        uint vertex_count;
        uint index_count;

        vertex_count=cci->field_count;
        index_count=(vertex_count-2)*3;

        if(!pc->Init("Circle",vertex_count,index_count))return(nullptr);

        VABMap3f vertex(pc->GetVABMap(VAN::Position));
        VABMap4f color(pc->GetVABMap(VAN::Color));
        VABMap3f normal(pc->GetVABMap(VAN::Normal));

        if(!vertex.IsValid())
            return(nullptr);

        for(uint i=0;i<cci->field_count+1;i++)
        {
            float ang=float(i)/float(cci->field_count)*360.0f;

            float x=cci->center.x+sin(deg2rad(ang))*cci->radius.x;
            float y=cci->center.y+cos(deg2rad(ang))*cci->radius.y;

            vertex->Write(x,y,0);
                
            if(color.IsValid())
                color->Write(cci->border_color);

            if(normal.IsValid())
                normal->Write(AxisVector::Z);
        }

        {
            IBMap *ib_map=pc->GetIBMap();

            if(pc->GetIndexType()==IndexType::U16)WriteCircleIBO<uint16>((uint16 *)(ib_map->Map()),cci->field_count);else
            if(pc->GetIndexType()==IndexType::U32)WriteCircleIBO<uint32>((uint32 *)(ib_map->Map()),cci->field_count);else
            if(pc->GetIndexType()==IndexType::U8 )WriteCircleIBO<uint8 >((uint8  *)(ib_map->Map()),cci->field_count);

            ib_map->Unmap();
        }

        return pc->Create();
    }
} // namespace
