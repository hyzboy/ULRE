#include "InlineGeometryCommon.h"
#include <hgl/math/geometry/BoundingVolumes.h>
#include <hgl/graph/geo/VKGeometry.h>

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
        if(!pc->Init("Circle",vertex_count,0))   // 非索引几何：无 IBO（gl_VertexIndex 直通）
            return(nullptr);

        auto vertex = pc->GetBufferAccessor<BufferAccessor2f>(VAN::Position);
        auto color  = pc->GetBufferAccessor<BufferAccessor4f>(VAN::Color);

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

        // Set bounding box for 2D circle
        float min_x = cci->center.x - cci->radius.x;
        float max_x = cci->center.x + cci->radius.x;
        float min_y = cci->center.y - cci->radius.y;
        float max_y = cci->center.y + cci->radius.y;

        return pc->CreateWithAABB(
            math::Vector3f(min_x, min_y, -0.01f),
            math::Vector3f(max_x, max_y, 0.01f));
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
        bool has_index = pc->hasIndex();

        if(!pc->Init("Circle",vertex_count,has_index ? vertex_count : 0))return(nullptr);

        auto vertex = pc->GetBufferAccessor<BufferAccessor3f>(VAN::Position);
        auto color  = pc->GetBufferAccessor<BufferAccessor4f>(VAN::Color);
        auto normal = pc->GetBufferAccessor<BufferAccessor3f>(VAN::Normal);

        // RG16F/RG8 压缩法线（octahedral）
        VAB *nrm_vab = pc->GetVAB(VAN::Normal);
        const bool nrm_rg8   = (nrm_vab && nrm_vab->GetFormat() == VK_FORMAT_R8G8_UNORM);
        const bool nrm_rg16f = (nrm_vab && nrm_vab->GetFormat() == VK_FORMAT_R16G16_SFLOAT);
        BufferAccessor2u8 normal2u8 = nrm_rg8   ? pc->GetBufferAccessor<BufferAccessor2u8>(VAN::Normal) : BufferAccessor2u8();
        BufferAccessor2hf normal2   = nrm_rg16f ? pc->GetBufferAccessor<BufferAccessor2hf>(VAN::Normal) : BufferAccessor2hf();

        if(!vertex.IsValid())
            return(nullptr);

        if(cci->has_center)
        {
            vertex->Write(cci->center.x,cci->center.y,0);

            if(color.IsValid())
                color->Write(cci->center_color);

            if(normal.IsValid())
                if(normal2u8.IsValid())
                    normal2u8->Write(QuantizeU8(0.0f), QuantizeU8(0.0f));
                else if(normal2.IsValid())
                    normal2->Write(FloatToHalf(0.0f), FloatToHalf(0.0f));
                else
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
                if(normal2u8.IsValid())
                    normal2u8->Write(QuantizeU8(0.0f), QuantizeU8(0.0f));
                else if(normal2.IsValid())
                    normal2->Write(FloatToHalf(0.0f), FloatToHalf(0.0f));
                else
                    normal->Write(math::AxisVector::Z);
        }

        if(has_index)
        {
            if (pc->GetIndexType() == IndexType::U32) {
                auto ib_accessor = pc->GetIndexAccessor<uint32>();
                IndexGenerator::WriteSequentialIndices<uint32>((uint32*)ib_accessor,0,vertex_count);
            }}

        Geometry *p = pc->Create();
        if(p)
        {
            // Set bounding box for 3D circle (flat in XY plane)
            BoundingVolumes bv;
            float min_x = cci->center.x - cci->radius.x;
            float max_x = cci->center.x + cci->radius.x;
            float min_y = cci->center.y - cci->radius.y;
            float max_y = cci->center.y + cci->radius.y;
            bv.SetFromAABB(math::Vector3f(min_x, min_y, -0.01f),
                          math::Vector3f(max_x, max_y, 0.01f));
            p->SetBoundingVolumes(bv);
        }
        return p;
    }

    Geometry *CreateCircle3DByIndexTriangles(GeometryCreater *pc,const CircleCreateInfo *cci)
    {
        if(!pc)return(nullptr);

        uint vertex_count;
        uint index_count;

        vertex_count = cci->field_count;
        index_count = (vertex_count - 2) * 3;

        if(!pc->Init("Circle",vertex_count,index_count))return(nullptr);

        auto vertex = pc->GetBufferAccessor<BufferAccessor3f>(VAN::Position);
        auto color  = pc->GetBufferAccessor<BufferAccessor4f>(VAN::Color);
        auto normal = pc->GetBufferAccessor<BufferAccessor3f>(VAN::Normal);

        // RG16F/RG8 压缩法线（octahedral）
        VAB *nrm_vab = pc->GetVAB(VAN::Normal);
        const bool nrm_rg8   = (nrm_vab && nrm_vab->GetFormat() == VK_FORMAT_R8G8_UNORM);
        const bool nrm_rg16f = (nrm_vab && nrm_vab->GetFormat() == VK_FORMAT_R16G16_SFLOAT);
        BufferAccessor2u8 normal2u8 = nrm_rg8   ? pc->GetBufferAccessor<BufferAccessor2u8>(VAN::Normal) : BufferAccessor2u8();
        BufferAccessor2hf normal2   = nrm_rg16f ? pc->GetBufferAccessor<BufferAccessor2hf>(VAN::Normal) : BufferAccessor2hf();

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
                if(normal2u8.IsValid())
                    normal2u8->Write(QuantizeU8(0.0f), QuantizeU8(0.0f));
                else if(normal2.IsValid())
                    normal2->Write(FloatToHalf(0.0f), FloatToHalf(0.0f));
                else
                    normal->Write(math::AxisVector::Z);
        }

        {
            if (pc->GetIndexType() == IndexType::U32) {
                auto ib = pc->GetIndexAccessor<uint32>();
                IndexGenerator::WriteCircleIndices<uint32>(ib, cci->field_count);
            }}

        // Set bounding box for 3D circle (flat in XY plane)
        float min_x = cci->center.x - cci->radius.x;
        float max_x = cci->center.x + cci->radius.x;
        float min_y = cci->center.y - cci->radius.y;
        float max_y = cci->center.y + cci->radius.y;

        return pc->CreateWithAABB(
            math::Vector3f(min_x, min_y, -0.01f),
            math::Vector3f(max_x, max_y, 0.01f));
    }
} // namespace
