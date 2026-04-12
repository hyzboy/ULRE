#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    Geometry *CreatePlaneGrid2D(GeometryCreater *pc,const PlaneGridCreateInfo *pgci)
    {
        if(!pc->Init("PlaneGrid",((pgci->grid_size.Width()+1)+(pgci->grid_size.Height()+1))*2,0))
            return(nullptr);

        auto vertex = pc->GetBufferAccessor<BufferAccessor2f>(VAN::Position);

        if(!vertex.IsValid())
            return(nullptr);

        const float right=float(pgci->grid_size.Width())/2.0f;
        const float left =-right;

        const float bottom=float(pgci->grid_size.Height())/2.0f;
        const float top   =-bottom;

        for(uint row=0;row<=pgci->grid_size.Height();row++)
        {
            vertex->WriteLine(  Vector2f(left ,top+row),
                                Vector2f(right,top+row));
        }

        for(uint col=0;col<=pgci->grid_size.Width();col++)
        {
            vertex->WriteLine(  Vector2f(left+col,top   ),
                                Vector2f(left+col,bottom));
        }

        auto lum = pc->GetBufferAccessor<BufferAccessor1un8>(VAN::Luminance);

        if(lum.IsValid())
        {
            for(uint row=0;row<=pgci->grid_size.Height();row++)
            {
                if((row%pgci->sub_count.Height())==0)
                {
                    lum->Write(pgci->sub_lum);
                    lum->Write(pgci->sub_lum);
                }
                else
                {
                    lum->Write(pgci->lum);
                    lum->Write(pgci->lum);
                }
            }

            for(uint col=0;col<=pgci->grid_size.Width();col++)
            {
                if((col%pgci->sub_count.Width())==0)
                {
                    lum->Write(pgci->sub_lum);
                    lum->Write(pgci->sub_lum);
                }
                else
                {
                    lum->Write(pgci->lum);
                    lum->Write(pgci->lum);
                }
            }
        }

        // Set bounding box for 2D plane grid
        return pc->CreateWithAABB(
            math::Vector3f(left, top, -0.01f),
            math::Vector3f(right, bottom, 0.01f));
    }

    Geometry *CreatePlaneGrid3D(GeometryCreater *pc,const PlaneGridCreateInfo *pgci)
    {
        if(!pc->Init("PlaneGrid",((pgci->grid_size.Width()+1)+(pgci->grid_size.Height()+1))*2,0))
            return(nullptr);

        auto vertex = pc->GetBufferAccessor<BufferAccessor3f>(VAN::Position);

        if(!vertex.IsValid())
            return(nullptr);

        const float right=float(pgci->grid_size.Width())/2.0f;
        const float left =-right;

        const float bottom=float(pgci->grid_size.Height())/2.0f;
        const float top   =-bottom;

        for(uint row=0;row<=pgci->grid_size.Height();row++)
        {
            vertex->WriteLine(  Vector3f(left ,top+row,0),
                                Vector3f(right,top+row,0));
        }

        for(uint col=0;col<=pgci->grid_size.Width();col++)
        {
            vertex->WriteLine(  Vector3f(left+col,top   ,0),
                                Vector3f(left+col,bottom,0));
        }

        auto lum = pc->GetBufferAccessor<BufferAccessor1u8>(VAN::Luminance);

        if(lum.IsValid())
        {
            for(uint row=0;row<=pgci->grid_size.Height();row++)
            {
                if((row%pgci->sub_count.Height())==0)
                {
                    lum->Write(pgci->sub_lum);
                    lum->Write(pgci->sub_lum);
                }
                else
                {
                    lum->Write(pgci->lum);
                    lum->Write(pgci->lum);
                }
            }

            for(uint col=0;col<=pgci->grid_size.Width();col++)
            {
                if((col%pgci->sub_count.Width())==0)
                {
                    lum->Write(pgci->sub_lum);
                    lum->Write(pgci->sub_lum);
                }
                else
                {
                    lum->Write(pgci->lum);
                    lum->Write(pgci->lum);
                }
            }
        }

        // Set bounding box for 3D plane grid (flat in XY plane at Z=0)
        return pc->CreateWithAABB(
            math::Vector3f(left, top, -0.01f),
            math::Vector3f(right, bottom, 0.01f));
    }

    Geometry *CreatePlaneSqaure(GeometryCreater *pc)
    {
        const   float           xy_vertices [] = { -0.5f,-0.5f,0.0f,  +0.5f,-0.5f,0.0f,    +0.5f,+0.5f,0.0f,    -0.5f,+0.5f,0.0f   };
                float           xy_tex_coord[] = {  0.0f, 0.0f,        1.0f, 0.0f,          1.0f, 1.0f,          0.0f, 1.0f        };
        const   Vector3f  xy_normal(0.0f,0.0f,1.0f);
        const   Vector3f  xy_tangent(1.0f,0.0f,0.0f);
        const   uint16          indices[]={0,1,2,0,2,3};

        if(!pc)return(nullptr);

        if(!pc->Init("Plane",4,6,IndexType::U16))
            return(nullptr);

        if(!pc->WriteVAB(VAN::Position,VF_V3F,xy_vertices))
            return(nullptr);

        auto format_writer = pc->GetFormatAwareWriter();

        {
            auto normal = pc->GetBufferAccessor<BufferAccessor3f>(VAN::Normal);

            if(normal.IsValid())
            {
                for(int i=0;i<4;i++)
                    format_writer.WriteNormal(xy_normal.x,xy_normal.y,xy_normal.z);
            }
        }

        {
            auto tangent = pc->GetBufferAccessor<BufferAccessor3f>(VAN::Tangent);

            if(tangent.IsValid())
            {
                for(int i=0;i<4;i++)
                    format_writer.WriteTangent(xy_tangent.x,xy_tangent.y,xy_tangent.z);
            }
        }

        {
            auto tex_coord = pc->GetBufferAccessor<BufferAccessor2f>(VAN::TexCoord);

            if(tex_coord.IsValid())
            {
                for(int i=0;i<4;i++)
                    format_writer.WriteUV(xy_tex_coord[i*2],xy_tex_coord[i*2+1]);
            }
        }

        pc->WriteIBO(indices);

        return pc->CreateWithAABB(
            math::Vector3f(-0.5f,-0.5f,0.0f),
            Vector3f(0.5f,0.5f,0.0f));
    }
} // namespace
