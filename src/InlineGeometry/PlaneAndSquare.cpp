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

        auto lum = pc->GetBufferAccessor<BufferAccessor1u8>(VAN::Luminance);

        if(lum.IsValid())
        {
            // Luminance 与 Position 顶点一一对应：每行线 2 顶点、每列线 2 顶点，
            // 主网格线（row%sub_count==0）用 sub_lum（加亮）、子网格线用 lum（暗线）。
            // 修复：之前只为主网格线写值（数量<顶点数）→ 其余顶点未初始化（0）
            for(uint row=0;row<=pgci->grid_size.Height();row++)
            {
                const uint8 lum_val=(row%pgci->sub_count.Height()==0)?pgci->sub_lum:pgci->lum;

                lum->Write(lum_val);
                lum->Write(lum_val);
            }

            for(uint col=0;col<=pgci->grid_size.Width();col++)
            {
                const uint8 lum_val=(col%pgci->sub_count.Width()==0)?pgci->sub_lum:pgci->lum;

                lum->Write(lum_val);
                lum->Write(lum_val);
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
            // Luminance 与 Position 顶点一一对应（同 CreatePlaneGrid2D 修复）
            for(uint row=0;row<=pgci->grid_size.Height();row++)
            {
                const uint8 lum_val=(row%pgci->sub_count.Height()==0)?pgci->sub_lum:pgci->lum;

                lum->Write(lum_val);
                lum->Write(lum_val);
            }

            for(uint col=0;col<=pgci->grid_size.Width();col++)
            {
                const uint8 lum_val=(col%pgci->sub_count.Width()==0)?pgci->sub_lum:pgci->lum;

                lum->Write(lum_val);
                lum->Write(lum_val);
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
        const   uint32          indices[]={0,1,2,0,2,3};

        if(!pc)return(nullptr);

        if(!pc->Init("Plane",4,6,IndexType::U32))
            return(nullptr);

        if(!pc->WriteVAB(VAN::Position,VF_V3F,xy_vertices))
            return(nullptr);

        {
            VAB *nrm_vab = pc->GetVAB(VAN::Normal);
            if(nrm_vab && nrm_vab->GetFormat() == VK_FORMAT_R8G8_UNORM)
            {
                // RG8 压缩法线（octahedral → uint8 量化）
                auto normal2u8 = pc->GetBufferAccessor<BufferAccessor2u8>(VAN::Normal);
                if(normal2u8.IsValid())
                {
                    float p, q;
                    EncodeOctahedralNormal(xy_normal.x, xy_normal.y, xy_normal.z, p, q);
                    for(int i = 0; i < 4; ++i)
                        normal2u8->Write(QuantizeU8(p), QuantizeU8(q));
                }
            }
            else if(nrm_vab && nrm_vab->GetFormat() == VK_FORMAT_R16G16_SFLOAT)
            {
                // RG16F 压缩法线（octahedral 编码）
                auto normal2 = pc->GetBufferAccessor<BufferAccessor2hf>(VAN::Normal);
                if(normal2.IsValid())
                {
                    float p, q;
                    EncodeOctahedralNormal(xy_normal.x, xy_normal.y, xy_normal.z, p, q);
                    for(int i = 0; i < 4; ++i)
                        normal2->Write(FloatToHalf(p), FloatToHalf(q));
                }
            }
        }

        {
            auto tangent = pc->GetBufferAccessor<BufferAccessor3f>(VAN::Tangent);

            if(tangent.IsValid())
                tangent->RepeatWrite(xy_tangent,4);
        }

        {
            auto tex_coord = pc->GetBufferAccessor<BufferAccessor2f>(VAN::TexCoord);

            if(tex_coord.IsValid())
                tex_coord->Write(xy_tex_coord,4);
        }

        pc->WriteIBO(indices);

        return pc->CreateWithAABB(
            math::Vector3f(-0.5f,-0.5f,0.0f),
            Vector3f(0.5f,0.5f,0.0f));
    }
} // namespace
