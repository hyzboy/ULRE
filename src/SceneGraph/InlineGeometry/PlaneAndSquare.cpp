#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    Geometry *CreatePlaneGrid2D(GeometryCreater *pc,const PlaneGridCreateInfo *pgci)
    {
        if(!pc->Init("PlaneGrid",((pgci->grid_size.Width()+1)+(pgci->grid_size.Height()+1))*2,0))
            return(nullptr);

        VABMap2f vertex(pc->GetVABMap(VAN::Position));

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
            vertex->WriteLine(Vector2f(left+col,top   ),
                                Vector2f(left+col,bottom));
        }

        VABMap1uf8 lum(pc->GetVABMap(VAN::Luminance));

        if(lum.IsValid())
        {
            for(uint row=0;row<=pgci->grid_size.Height();row++)
            {
                if((row%pgci->sub_count.Height())==0)
                    lum->RepeatWrite(pgci->sub_lum,2);
                else
                    lum->RepeatWrite(pgci->lum,2);
            }

            for(uint col=0;col<=pgci->grid_size.Width();col++)
            {
                if((col%pgci->sub_count.Width())==0)
                    lum->RepeatWrite(pgci->sub_lum,2);
                else
                    lum->RepeatWrite(pgci->lum,2);
            }
        }

        return pc->Create();
    }

    Geometry *CreatePlaneGrid3D(GeometryCreater *pc,const PlaneGridCreateInfo *pgci)
    {
        if(!pc->Init("PlaneGrid",((pgci->grid_size.Width()+1)+(pgci->grid_size.Height()+1))*2,0))
            return(nullptr);

        VABMap3f vertex(pc->GetVABMap(VAN::Position));

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
            vertex->WriteLine(Vector3f(left+col,top   ,0),
                                Vector3f(left+col,bottom,0));
        }

        VABMap1uf8 lum(pc->GetVABMap(VAN::Luminance));

        if(lum.IsValid())
        {
            for(uint row=0;row<=pgci->grid_size.Height();row++)
            {
                if((row%pgci->sub_count.Height())==0)
                    lum->RepeatWrite(pgci->sub_lum,2);
                else
                    lum->RepeatWrite(pgci->lum,2);
            }

            for(uint col=0;col<=pgci->grid_size.Width();col++)
            {
                if((col%pgci->sub_count.Width())==0)
                    lum->RepeatWrite(pgci->sub_lum,2);
                else
                    lum->RepeatWrite(pgci->lum,2);
            }
        }

        return pc->Create();
    }

    Geometry *CreatePlaneSqaure(GeometryCreater *pc)
    {
        const   float       xy_vertices [] = { -0.5f,-0.5f,0.0f,  +0.5f,-0.5f,0.0f,    +0.5f,+0.5f,0.0f,    -0.5f,+0.5f,0.0f   };
                float       xy_tex_coord[] = {  0.0f, 0.0f,        1.0f, 0.0f,          1.0f, 1.0f,          0.0f, 1.0f        };
        const   Vector3f    xy_normal(0.0f,0.0f,1.0f);
        const   Vector3f    xy_tangent(1.0f,0.0f,0.0f);
        const   uint16      indices[]={0,1,2,0,2,3};

        if(!pc)return(nullptr);

        if(!pc->Init("Plane",4,6,IndexType::U16))
            return(nullptr);

        if(!pc->WriteVAB(VAN::Position,VF_V3F,xy_vertices))
            return(nullptr);

        {
            VABMap3f normal(pc->GetVABMap(VAN::Normal));

            if(normal.IsValid())
                normal->RepeatWrite(xy_normal,4);
        }

        {
            VABMap3f tangent(pc->GetVABMap(VAN::Tangent));

            if(tangent.IsValid())
                tangent->RepeatWrite(xy_tangent,4);
        }

        {
            VABMap2f tex_coord(pc->GetVABMap(VAN::TexCoord));

            if(tex_coord.IsValid())
                tex_coord->Write(xy_tex_coord,4);
        }

        pc->WriteIBO(indices);

        Geometry *p=pc->Create();

        BoundingVolumes bv;

        bv.SetFromAABB(Vector3f(-0.5f,-0.5f,0.0f),Vector3f(0.5f,0.5f,0.0f));

        p->SetBoundingVolumes(bv);

        return p;
    }
} // namespace
