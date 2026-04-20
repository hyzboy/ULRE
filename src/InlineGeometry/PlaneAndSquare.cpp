#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    Geometry *CreatePlaneGrid2D(GeometryCreater *pc,const PlaneGridCreateInfo *pgci)
    {
        if(!pc->Init("PlaneGrid",((pgci->grid_size.Width()+1)+(pgci->grid_size.Height()+1))*2,0))
            return(nullptr);

        GeometryBuilder builder(pc);

        if(!builder.IsValid())
            return(nullptr);

        const float right=float(pgci->grid_size.Width())/2.0f;
        const float left =-right;

        const float bottom=float(pgci->grid_size.Height())/2.0f;
        const float top   =-bottom;

        for(uint row=0;row<=pgci->grid_size.Height();row++)
        {
            builder.WriteVertex(left,  top + row);
            builder.WriteVertex(right, top + row);
        }

        for(uint col=0;col<=pgci->grid_size.Width();col++)
        {
            builder.WriteVertex(left + col, top);
            builder.WriteVertex(left + col, bottom);
        }

        if(builder.HasLuminance())
        {
            for(uint row=0;row<=pgci->grid_size.Height();row++)
            {
                if((row%pgci->sub_count.Height())==0)
                {
                    builder.WriteLuminance(pgci->sub_lum);
                    builder.WriteLuminance(pgci->sub_lum);
                }
                else
                {
                    builder.WriteLuminance(pgci->lum);
                    builder.WriteLuminance(pgci->lum);
                }
            }

            for(uint col=0;col<=pgci->grid_size.Width();col++)
            {
                if((col%pgci->sub_count.Width())==0)
                {
                    builder.WriteLuminance(pgci->sub_lum);
                    builder.WriteLuminance(pgci->sub_lum);
                }
                else
                {
                    builder.WriteLuminance(pgci->lum);
                    builder.WriteLuminance(pgci->lum);
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

        GeometryBuilder builder(pc);

        if(!builder.IsValid())
            return(nullptr);

        const float right=float(pgci->grid_size.Width())/2.0f;
        const float left =-right;

        const float bottom=float(pgci->grid_size.Height())/2.0f;
        const float top   =-bottom;

        for(uint row=0;row<=pgci->grid_size.Height();row++)
        {
            builder.WriteVertex(left,  top + row, 0.0f);
            builder.WriteVertex(right, top + row, 0.0f);
        }

        for(uint col=0;col<=pgci->grid_size.Width();col++)
        {
            builder.WriteVertex(left + col, top, 0.0f);
            builder.WriteVertex(left + col, bottom, 0.0f);
        }

        if(builder.HasLuminance())
        {
            for(uint row=0;row<=pgci->grid_size.Height();row++)
            {
                if((row%pgci->sub_count.Height())==0)
                {
                    builder.WriteLuminance(pgci->sub_lum);
                    builder.WriteLuminance(pgci->sub_lum);
                }
                else
                {
                    builder.WriteLuminance(pgci->lum);
                    builder.WriteLuminance(pgci->lum);
                }
            }

            for(uint col=0;col<=pgci->grid_size.Width();col++)
            {
                if((col%pgci->sub_count.Width())==0)
                {
                    builder.WriteLuminance(pgci->sub_lum);
                    builder.WriteLuminance(pgci->sub_lum);
                }
                else
                {
                    builder.WriteLuminance(pgci->lum);
                    builder.WriteLuminance(pgci->lum);
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
        const   Vector3f  xy_normal(0.0f,0.0f,1.0f);
        const   Vector3f  xy_tangent(1.0f,0.0f,0.0f);
        const   uint16          indices[]={0,1,2,0,2,3};

        if(!pc)return(nullptr);

        if(!pc->Init("Plane",4,6,IndexType::U16))
            return(nullptr);

        GeometryBuilder builder(pc);

        if(!builder.IsValid())
            return(nullptr);

        builder.WriteVertex(-0.5f, -0.5f, 0.0f);
        if(builder.HasNormals())
            builder.WriteNormal(xy_normal.x, xy_normal.y, xy_normal.z);
        if(builder.HasTangents())
            builder.WriteTangent(xy_tangent.x, xy_tangent.y, xy_tangent.z, 1.0f); // default tangent.w = +1; TODO: compute handedness when tangent basis data is available
        if(builder.HasTexCoords())
            builder.WriteTexCoord(0.0f, 0.0f);

        builder.WriteVertex(0.5f, -0.5f, 0.0f);
        if(builder.HasNormals())
            builder.WriteNormal(xy_normal.x, xy_normal.y, xy_normal.z);
        if(builder.HasTangents())
            builder.WriteTangent(xy_tangent.x, xy_tangent.y, xy_tangent.z, 1.0f); // default tangent.w = +1; TODO: compute handedness when tangent basis data is available
        if(builder.HasTexCoords())
            builder.WriteTexCoord(1.0f, 0.0f);

        builder.WriteVertex(0.5f, 0.5f, 0.0f);
        if(builder.HasNormals())
            builder.WriteNormal(xy_normal.x, xy_normal.y, xy_normal.z);
        if(builder.HasTangents())
            builder.WriteTangent(xy_tangent.x, xy_tangent.y, xy_tangent.z, 1.0f); // default tangent.w = +1; TODO: compute handedness when tangent basis data is available
        if(builder.HasTexCoords())
            builder.WriteTexCoord(1.0f, 1.0f);

        builder.WriteVertex(-0.5f, 0.5f, 0.0f);
        if(builder.HasNormals())
            builder.WriteNormal(xy_normal.x, xy_normal.y, xy_normal.z);
        if(builder.HasTangents())
            builder.WriteTangent(xy_tangent.x, xy_tangent.y, xy_tangent.z, 1.0f); // default tangent.w = +1; TODO: compute handedness when tangent basis data is available
        if(builder.HasTexCoords())
            builder.WriteTexCoord(0.0f, 1.0f);

        pc->WriteIBO(indices);

        return pc->CreateWithAABB(
            math::Vector3f(-0.5f,-0.5f,0.0f),
            Vector3f(0.5f,0.5f,0.0f));
    }
} // namespace
