#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    Geometry *CreateCircle2D(GeometryCreater *pc,const CircleCreateInfo *cci)
    {
        if(!pc)return(nullptr);

        uint edge;
        uint vertex_count;

        if(cci->has_center)
        {
            edge = cci->field_count + 1;
            vertex_count = cci->field_count + 2;
        }
        else
        {
            // Need one extra vertex to close the loop (duplicate of the first)
            edge = cci->field_count + 1;
            vertex_count = cci->field_count + 1;
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

        for(uint i = 0;i < edge;i++)
        {
            float ang = float(i) / float(cci->field_count) * 360.0f;

            float x = cci->center.x + sin(deg2rad(ang)) * cci->radius.x;
            float y = cci->center.y + cos(deg2rad(ang)) * cci->radius.y;

            vertex->Write(x,y);

            if(color.IsValid())
                color->Write(cci->border_color);
        }

        return pc->Create();
    }

    Geometry *CreateCircle3D(GeometryCreater *pc,const CircleCreateInfo *cci)
    {
        if(!pc)return(nullptr);

        uint edge;
        uint vertex_count;

        if(cci->has_center)
        {
            edge = cci->field_count + 1;
            vertex_count = cci->field_count + 2;
        }
        else
        {
            // mirror 2D: need one extra vertex to close the loop
            edge = cci->field_count + 1;
            vertex_count = cci->field_count + 1;
        }

        bool has_index = pc->hasIndex();

        if(!pc->Init("Circle",vertex_count,has_index ? vertex_count : 0))return(nullptr);

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
                normal->Write(math::AxisVector::Z);
        }

        for(uint i = 0;i < edge;i++)
        {
            float ang = float(i) / float(cci->field_count) * 360.0f;

            float x = cci->center.x + sin(deg2rad(ang)) * cci->radius.x;
            float y = cci->center.y + cos(deg2rad(ang)) * cci->radius.y;

            vertex->Write(x,y,0);

            if(color.IsValid())
                color->Write(cci->border_color);

            if(normal.IsValid())
                normal->Write(math::AxisVector::Z);
        }

        if(has_index)
        {
            IBMap *ib_map = pc->GetIBMap();

            if(pc->GetIndexType() == IndexType::U16)IndexGenerator::WriteSequentialIndices<uint16>((uint16 *)(ib_map->Map()),0,vertex_count);else
                if(pc->GetIndexType() == IndexType::U32)IndexGenerator::WriteSequentialIndices<uint32>((uint32 *)(ib_map->Map()),0,vertex_count);else
                    if(pc->GetIndexType() == IndexType::U8)IndexGenerator::WriteSequentialIndices<uint8 >((uint8 *)(ib_map->Map()),0,vertex_count);

            ib_map->Unmap();
        }

        return pc->Create();
    }

    Geometry *CreateCircle3DByIndexTriangles(GeometryCreater *pc,const CircleCreateInfo *cci)
    {
        if(!pc)return(nullptr);

        uint vertex_count;
        uint index_count;

        vertex_count = cci->field_count;
        index_count = (vertex_count - 2) * 3;

        if(!pc->Init("Circle",vertex_count,index_count))return(nullptr);

        VABMap3f vertex(pc->GetVABMap(VAN::Position));
        VABMap4f color(pc->GetVABMap(VAN::Color));
        VABMap3f normal(pc->GetVABMap(VAN::Normal));

        if(!vertex.IsValid())
            return(nullptr);

        // write exactly vertex_count vertices (no duplicate)
        for(uint i = 0;i < cci->field_count;i++)
        {
            float ang = float(i) / float(cci->field_count) * 360.0f;

            float x = cci->center.x + sin(deg2rad(ang)) * cci->radius.x;
            float y = cci->center.y + cos(deg2rad(ang)) * cci->radius.y;

            vertex->Write(x,y,0);

            if(color.IsValid())
                color->Write(cci->border_color);

            if(normal.IsValid())
                normal->Write(math::AxisVector::Z);
        }

        {
            IBMap *ib_map = pc->GetIBMap();

            if(pc->GetIndexType() == IndexType::U16)IndexGenerator::WriteCircleIndices<uint16>((uint16 *)(ib_map->Map()),cci->field_count);else
                if(pc->GetIndexType() == IndexType::U32)IndexGenerator::WriteCircleIndices<uint32>((uint32 *)(ib_map->Map()),cci->field_count);else
                    if(pc->GetIndexType() == IndexType::U8)IndexGenerator::WriteCircleIndices<uint8 >((uint8 *)(ib_map->Map()),cci->field_count);

            ib_map->Unmap();
        }

        return pc->Create();
    }
} // namespace
