// sphere、cylinear、cone、tours code from McNopper,website: https://github.com/McNopper/GLUS
// GL to VK: swap Y/Z of position/normal/tangent/index

#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/VertexAttribDataAccess.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKShaderModule.h>
#include<hgl/graph/PrimitiveCreater.h>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace hgl::graph::inline_geometry
{
    Primitive *CreateRectangle(PrimitiveCreater *pc,const RectangleCreateInfo *rci)
    {
        if(!pc)return(nullptr);

        if(!pc->Init("Rectangle",4,0))
            return(nullptr);

        VABMap2f vertex(pc->GetVABMap(VAN::Position));

        if(!vertex.IsValid())
            return(nullptr);

        vertex->WriteRectFan(rci->scope);

        return pc->Create();
    }

    Primitive *CreateGBufferCompositionRectangle(PrimitiveCreater *pc)
    {
        RectangleCreateInfo rci;

        rci.scope.Set(-1,-1,2,2);

        return CreateRectangle(pc,&rci);
    }

    Primitive *CreateRoundRectangle(PrimitiveCreater *pc,const RoundRectangleCreateInfo *rci)
    {
        if(!pc)return(nullptr);

        if(rci->radius==0||rci->round_per<=1)      //这是要画矩形
        {
            if(!pc->Init("RoundRectangle",4,0))
                return(nullptr);
                    
            VABMap2f vertex(pc->GetVABMap(VAN::Position));

            vertex->WriteRectFan(rci->scope);
        }
        else
        {
            float radius=rci->radius;

            if(radius>rci->scope.GetWidth()/2.0f)radius=rci->scope.GetWidth()/2.0f;
            if(radius>rci->scope.GetHeight()/2.0f)radius=rci->scope.GetHeight()/2.0f;

            if(!pc->Init("RoundRectangle",rci->round_per*4,8))
                return(nullptr);
                
            VABMap2f vertex(pc->GetVABMap(VAN::Position));

            Vector2f *coord=new Vector2f[rci->round_per];

            float   l=rci->scope.GetLeft(),
                    r=rci->scope.GetRight(),
                    t=rci->scope.GetTop(),
                    b=rci->scope.GetBottom();

            for(uint i=0;i<rci->round_per;i++)
            {
                float ang=float(i)/float(rci->round_per-1)*90.0f;

                float x=sin(deg2rad(ang))*radius;
                float y=cos(deg2rad(ang))*radius;

                coord[i].x=x;
                coord[i].y=y;

                //右上角
                vertex->Write(r-radius+x,
                                t+radius-y);
            }

            //右下角
            for(uint i=0;i<rci->round_per;i++)
            {
                vertex->Write(r-radius+coord[rci->round_per-1-i].x,
                                b-radius+coord[rci->round_per-1-i].y);
            }

            //左下角
            for(uint i=0;i<rci->round_per;i++)
            {
                vertex->Write(l+radius-coord[i].x,
                                b-radius+coord[i].y);
            }

            //左上角
            for(uint i=0;i<rci->round_per;i++)
            {
                vertex->Write(l+radius-coord[rci->round_per-1-i].x,
                                t+radius-coord[rci->round_per-1-i].y);
            }

            delete[] coord;
        }

        return pc->Create();
    }

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

    template<typename T>
    void WriteIBO(T *p,const T start,const T count)
    {
        for(T i=start;i<start+count;i++)
        {
            *p=i;
            ++p;
        }
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

    template<typename T>
    void WriteCircleIBO(T *ibo,const uint edge_count)
    {
        T *p=ibo;

        for(T i=1;i<edge_count;i++)
        {
            *p=0;  ++p;
            *p=i+1;++p;
            *p=i;  ++p;
        }

        *p=0;           ++p;
        *p=1;           ++p;
        *p=edge_count;
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
            
    Primitive *CreatePlaneGrid2D(PrimitiveCreater *pc,const PlaneGridCreateInfo *pgci)
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

    Primitive *CreatePlaneGrid3D(PrimitiveCreater *pc,const PlaneGridCreateInfo *pgci)
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

    Primitive *CreatePlaneSqaure(PrimitiveCreater *pc)
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

        Primitive *p=pc->Create();

        {
            AABB aabb;

            aabb.SetMinMax(Vector3f(-0.5f,-0.5f,0.0f),Vector3f(0.5f,0.5f,0.0f));

            p->SetBoundingBox(aabb);
        }

        return p;
    }

    Primitive *CreateCube(PrimitiveCreater *pc,const CubeCreateInfo *cci)
    {   
        /**
            *     4            5 
            *     *------------*       Z               Y
            *    /|           /|       |    Y          |    Z
            *  0/ |         1/ |       |   /           |   /
            *  *--+---------*  |       |  /            |  /
            *  |  |         |  |       | /             | /
            *  | 7|         | 6|       |/              |/
            *  |  *---------+--*       *-----------X   *-----------X
            *  | /          | /  
            *  |/          2|/             my              Cubemap
            * 3*------------*    
            * 
            * 注：cubemap纹理坐标系依然遵循OpenGL时代定下的坐标系，所以这里的position虽然使用vulkan坐标系，但在shader中当做cubemap纹理使用时，需要在shader中转换为opengl坐标系(交换yz即可)
            */

        constexpr float positions[]={   -0.5f, -0.5f, -0.5f,    -0.5f, -0.5f, +0.5f,    +0.5f, -0.5f, +0.5f,    +0.5f, -0.5f, -0.5f,
                                        -0.5f, +0.5f, -0.5f,    -0.5f, +0.5f, +0.5f,    +0.5f, +0.5f, +0.5f,    +0.5f, +0.5f, -0.5f,
                                        -0.5f, -0.5f, -0.5f,    -0.5f, +0.5f, -0.5f,    +0.5f, +0.5f, -0.5f,    +0.5f, -0.5f, -0.5f,
                                        -0.5f, -0.5f, +0.5f,    -0.5f, +0.5f, +0.5f,    +0.5f, +0.5f, +0.5f,    +0.5f, -0.5f, +0.5f,
                                        -0.5f, -0.5f, -0.5f,    -0.5f, -0.5f, +0.5f,    -0.5f, +0.5f, +0.5f,    -0.5f, +0.5f, -0.5f,
                                        +0.5f, -0.5f, -0.5f,    +0.5f, -0.5f, +0.5f,    +0.5f, +0.5f, +0.5f,    +0.5f, +0.5f, -0.5f};

        constexpr float normals[]={     +0.0f, +1.0f, +0.0f,    +0.0f, +1.0f, +0.0f,    +0.0f, +1.0f, +0.0f,    +0.0f, +1.0f, +0.0f,
                                        +0.0f, -1.0f, +0.0f,    +0.0f, -1.0f, +0.0f,    +0.0f, -1.0f, +0.0f,    +0.0f, -1.0f, +0.0f,
                                        +0.0f, -0.0f, -1.0f,    +0.0f, -0.0f, -1.0f,    +0.0f, -0.0f, -1.0f,    +0.0f, -0.0f, -1.0f,
                                        +0.0f, -0.0f, +1.0f,    +0.0f, -0.0f, +1.0f,    +0.0f, -0.0f, +1.0f,    +0.0f, -0.0f, +1.0f,
                                        -1.0f, -0.0f, +0.0f,    -1.0f, -0.0f, +0.0f,    -1.0f, -0.0f, +0.0f,    -1.0f, -0.0f, +0.0f,
                                        +1.0f, -0.0f, +0.0f,    +1.0f, -0.0f, +0.0f,    +1.0f, -0.0f, +0.0f,    +1.0f, -0.0f, +0.0f};

        constexpr float tangents[] = {  +1.0f, 0.0f, 0.0f,      +1.0f, 0.0f, 0.0f,      +1.0f, 0.0f, 0.0f,      +1.0f, 0.0f, 0.0f,
                                        +1.0f, 0.0f, 0.0f,      +1.0f, 0.0f, 0.0f,      +1.0f, 0.0f, 0.0f,      +1.0f, 0.0f, 0.0f,
                                        -1.0f, 0.0f, 0.0f,      -1.0f, 0.0f, 0.0f,      -1.0f, 0.0f, 0.0f,      -1.0f, 0.0f, 0.0f,
                                        +1.0f, 0.0f, 0.0f,      +1.0f, 0.0f, 0.0f,      +1.0f, 0.0f, 0.0f,      +1.0f, 0.0f, 0.0f,
                                            0.0f, 0.0f,+1.0f,       0.0f, 0.0f,+1.0f,       0.0f, 0.0f,+1.0f,       0.0f, 0.0f,+1.0f,
                                            0.0f, 0.0f,-1.0f,       0.0f, 0.0f,-1.0f,       0.0f, 0.0f,-1.0f,       0.0f, 0.0f,-1.0f};

        constexpr float tex_coords[] ={ 0.0f, 0.0f,     0.0f, 1.0f,     1.0f, 1.0f,     1.0f, 0.0f,
                                        0.0f, 1.0f,     0.0f, 0.0f,     1.0f, 0.0f,     1.0f, 1.0f,
                                        1.0f, 0.0f,     1.0f, 1.0f,     0.0f, 1.0f,     0.0f, 0.0f,
                                        0.0f, 0.0f,     0.0f, 1.0f,     1.0f, 1.0f,     1.0f, 0.0f,
                                        0.0f, 0.0f,     1.0f, 0.0f,     1.0f, 1.0f,     0.0f, 1.0f,
                                        1.0f, 0.0f,     0.0f, 0.0f,     0.0f, 1.0f,     1.0f, 1.0f};
        // The associated indices.
        constexpr uint16 indices[]={    0,    2,    1,       0,    3,    2,
                                        4,    5,    6,       4,    6,    7,
                                        8,    9,   10,       8,   10,   11,
                                        12,  15,   14,      12,   14,   13,
                                        16,  17,   18,      16,   18,   19,
                                        20,  23,   22,      20,   22,   21};

        if(!pc)return(nullptr);

        if(!pc->Init("Cube",24,6*2*3,IndexType::U16))
            return(nullptr);

        if(!pc->WriteVAB(VAN::Position,VF_V3F,positions))
            return(nullptr);

        if(cci->normal)
            if(!pc->WriteVAB(VAN::Normal,VF_V3F,normals))
                return(nullptr);

        if(cci->tangent)
            if(!pc->WriteVAB(VAN::Tangent,VF_V3F,tangents))
                return(nullptr);

        if(cci->tex_coord)
            if(!pc->WriteVAB(VAN::TexCoord,VF_V2F,tex_coords))
                return(nullptr);

        if(cci->color_type!=CubeCreateInfo::ColorType::NoColor)
        {                
            RANGE_CHECK_RETURN_NULLPTR(cci->color_type);

            VABMap4f color(pc->GetVABMap(VAN::Color));

            if(color.IsValid())
            {
                if(cci->color_type==CubeCreateInfo::ColorType::SameColor)
                    color->RepeatWrite(cci->color[0],24);
                else
                if(cci->color_type==CubeCreateInfo::ColorType::FaceColor)
                {
                    for(uint face=0;face<6;face++)
                        color->RepeatWrite(cci->color[face],4);
                }
                else
                if(cci->color_type==CubeCreateInfo::ColorType::VertexColor)
                    color->Write(cci->color,24);
                else
                    return(nullptr);
            }
        }

        pc->WriteIBO(indices);

        Primitive *p=pc->Create();

        p->SetBoundingBox(Vector3f(-0.5f,-0.5f,-0.5f),Vector3f(0.5f,0.5f,0.5f));

        return p;
    }

    template<typename T> 
    void CreateSphereIndices(PrimitiveCreater *pc,uint numberParallels,const uint numberSlices)
    {
        IBTypeMap<T> ib_map(pc->GetIBMap());
        T *tp=ib_map;

        for (uint i = 0; i < numberParallels; i++)
        {
            for (uint j = 0; j < numberSlices; j++)
            {
                *tp= i      * (numberSlices + 1) + j;       ++tp;
                *tp=(i + 1) * (numberSlices + 1) + (j + 1); ++tp;
                *tp=(i + 1) * (numberSlices + 1) + j;       ++tp;

                *tp= i      * (numberSlices + 1) + j;       ++tp;
                *tp= i      * (numberSlices + 1) + (j + 1); ++tp;
                *tp=(i + 1) * (numberSlices + 1) + (j + 1); ++tp;
            }
        }
    }

    namespace
    {
        constexpr uint GLUS_VERTICES_FACTOR =4;
        constexpr uint GLUS_VERTICES_DIVISOR=4;
        constexpr uint GLUS_MAX_VERTICES    =1048576;
        constexpr uint GLUS_MAX_INDICES     =GLUS_MAX_VERTICES*GLUS_VERTICES_FACTOR;

        void glusQuaternionRotateRyf(float quaternion[4], const float angle)
        {
            float halfAngleRadian = deg2rad(angle) * 0.5f;

            quaternion[0] = 0.0f;
            quaternion[1] = sin(halfAngleRadian);
            quaternion[2] = 0.0f;
            quaternion[3] = cos(halfAngleRadian);
        }

        void glusQuaternionRotateRzf(float quaternion[4], const float angle)
        {
            float halfAngleRadian = deg2rad(angle) * 0.5f;

            quaternion[0] = 0.0f;
            quaternion[1] = 0.0f;
            quaternion[2] = sin(halfAngleRadian);
            quaternion[3] = cos(halfAngleRadian);
        }

        void glusQuaternionGetMatrix4x4f(float matrix[16], const float quaternion[4])
        {
            float x = quaternion[0];
            float y = quaternion[1];
            float z = quaternion[2];
            float w = quaternion[3];

            matrix[0] = 1.0f - 2.0f * y * y - 2.0f * z * z;
            matrix[1] = 2.0f * x * y + 2.0f * w * z;
            matrix[2] = 2.0f * x * z - 2.0f * w * y;
            matrix[3] = 0.0f;

            matrix[4] = 2.0f * x * y - 2.0f * w * z;
            matrix[5] = 1.0f - 2.0f * x * x - 2.0f * z * z;
            matrix[6] = 2.0f * y * z + 2.0f * w * x;
            matrix[7] = 0.0f;

            matrix[8] = 2.0f * x * z + 2.0f * w * y;
            matrix[9] = 2.0f * y * z - 2.0f * w * x;
            matrix[10] = 1.0f - 2.0f * x * x - 2.0f * y * y;
            matrix[11] = 0.0f;

            matrix[12] = 0.0f;
            matrix[13] = 0.0f;
            matrix[14] = 0.0f;
            matrix[15] = 1.0f;
        }

        void glusMatrix4x4MultiplyVector3f(float result[3], const float matrix[16], const float vector[3])
        {
            int i;

            float temp[3];

            for (i = 0; i < 3; i++)
            {
                temp[i] = matrix[i] * vector[0] + matrix[4 + i] * vector[1] + matrix[8 + i] * vector[2];
            }

            for (i = 0; i < 3; i++)
            {
                result[i] = temp[i];
            }
        }
    }//namespace

    /**
    * 创建一个球体的可渲染数据,球心为0,0,0，半径为1
    * @param numberSlices 切片数
    * @return 可渲染数据
    */
    Primitive *CreateSphere(PrimitiveCreater *pc,const uint numberSlices)
    {
        uint numberParallels = (numberSlices+1) / 2;
        uint numberVertices = (numberParallels + 1) * (numberSlices + 1);
        uint numberIndices = numberParallels * numberSlices * 6;

        const double angleStep = double(2.0f * HGL_PI) / ((double) numberSlices);

        // used later to help us calculating tangents vectors
        float helpVector[3] = { 1.0f, 0.0f, 0.0f };
        float helpQuaternion[4];
        float helpMatrix[16];
        float tex_x;

        if(!pc->Init("Sphere",numberVertices,numberIndices))
            return(nullptr);

        VABMapFloat vertex   (pc->GetVABMap(VAN::Position),VF_V3F);
        VABMapFloat normal   (pc->GetVABMap(VAN::Normal),VF_V3F);
        VABMapFloat tangent  (pc->GetVABMap(VAN::Tangent),VF_V3F);
        VABMapFloat tex_coord(pc->GetVABMap(VAN::TexCoord),VF_V2F);

        float *vp=vertex;
        float *np=normal;
        float *tp=tangent;
        float *tcp=tex_coord;

        if(!vp)
            return(nullptr);

        for (uint i = 0; i < numberParallels + 1; i++)
        {
            for (uint j = 0; j < numberSlices + 1; j++)
            {
                float x = sin(angleStep * (double) i) * sin(angleStep * (double) j);
                float y = sin(angleStep * (double) i) * cos(angleStep * (double) j);
                float z = cos(angleStep * (double) i);

                *vp=x;++vp;
                *vp=y;++vp;
                *vp=z;++vp;

                if(np)
                {
                    *np=x;++np;
                    *np=y;++np;
                    *np=z;++np;
                }

                if(tcp)
                {
                    tex_x=(float) j / (float) numberSlices;

                    *tcp=tex_x;++tcp;
                    *tcp=1.0f - (float) i / (float) numberParallels;++tcp;

                    if(tp)
                    {
                        // use quaternion to get the tangent vector
                        glusQuaternionRotateRyf(helpQuaternion, 360.0f * tex_x);
                        glusQuaternionGetMatrix4x4f(helpMatrix, helpQuaternion);

                        glusMatrix4x4MultiplyVector3f(tp, helpMatrix, helpVector);
                        tp+=3;
                    }
                }
            }
        }

        //索引
        {
            const IndexType index_type=pc->GetIndexType();

            if(index_type==IndexType::U16)CreateSphereIndices<uint16>(pc,numberParallels,numberSlices);else
            if(index_type==IndexType::U32)CreateSphereIndices<uint32>(pc,numberParallels,numberSlices);else
            if(index_type==IndexType::U8 )CreateSphereIndices<uint8 >(pc,numberParallels,numberSlices);else
                return(nullptr);
        }

        Primitive *p=pc->Create();

        {
            AABB aabb;
            aabb.SetMinMax(Vector3f(-1.0f,-1.0f,-1.0f),Vector3f(1.0f,1.0f,1.0f));
            p->SetBoundingBox(aabb);
        }

        return p;
    }

    Primitive *CreateDome(PrimitiveCreater *pc,const uint numberSlices)
    {
        if(!pc)return(nullptr);

        uint numberParallels = numberSlices / 4;
        uint numberVertices = (numberParallels + 1) * (numberSlices + 1);
        uint numberIndices = numberParallels * numberSlices * 6;

        float angleStep = (2.0f * HGL_PI) / ((float) numberSlices);

        // used later to help us calculating tangents vectors
        float helpVector[3] = { 1.0f, 0.0f, 0.0f };
        float helpQuaternion[4];
        float helpMatrix[16];
        float tex_x;

        if (numberSlices < 3 || numberVertices > GLUS_MAX_VERTICES || numberIndices > GLUS_MAX_INDICES)
            return nullptr;

        if(!pc->Init("Dome",numberVertices,numberIndices))
            return(nullptr);
                
        VABMapFloat vertex   (pc->GetVABMap(VAN::Position),VF_V3F);
        VABMapFloat normal   (pc->GetVABMap(VAN::Normal),VF_V3F);
        VABMapFloat tangent  (pc->GetVABMap(VAN::Tangent),VF_V3F);
        VABMapFloat tex_coord(pc->GetVABMap(VAN::TexCoord),VF_V2F);

        float *vp=vertex;
        float *np=normal;
        float *tp=tangent;
        float *tcp=tex_coord;

        if(!vp)
            return(nullptr);

        for (uint i = 0; i < numberParallels + 1; i++)
        {
            for (uint j = 0; j < numberSlices + 1; j++)
            {
                uint vertexIndex = (i * (numberSlices + 1) + j) * 4;
                uint normalIndex = (i * (numberSlices + 1) + j) * 3;
                uint tangentIndex = (i * (numberSlices + 1) + j) * 3;
                uint texCoordsIndex = (i * (numberSlices + 1) + j) * 2;
                    
                float x = sin(angleStep * (double) i) * sin(angleStep * (double) j);
                float y = sin(angleStep * (double) i) * cos(angleStep * (double) j);
                float z = cos(angleStep * (double) i);

                *vp=x;++vp;
                *vp=y;++vp;
                *vp=z;++vp;

                if(np)
                {
                    *np=+x;++np;
                    *np=-y;++np;
                    *np=+z;++np;
                }

                if(tcp)
                {                        
                    tex_x=(float) j / (float) numberSlices;

                    *tcp=tex_x;++tcp;
                    *tcp=1.0f - (float) i / (float) numberParallels;++tcp;

                    if(tp)
                    {
                        // use quaternion to get the tangent vector
                        glusQuaternionRotateRyf(helpQuaternion, 360.0f * tex_x);
                        glusQuaternionGetMatrix4x4f(helpMatrix, helpQuaternion);

                        glusMatrix4x4MultiplyVector3f(tp, helpMatrix, helpVector);
                        tp+=3;
                    }
                }
            }
        }

        //索引
        {
            const IndexType index_type=pc->GetIndexType();

            if(index_type==IndexType::U16)CreateSphereIndices<uint16>(pc,numberParallels,numberSlices);else
            if(index_type==IndexType::U32)CreateSphereIndices<uint32>(pc,numberParallels,numberSlices);else
            if(index_type==IndexType::U8 )CreateSphereIndices<uint8 >(pc,numberParallels,numberSlices);else
                return(nullptr);
        }

        Primitive *p=pc->Create();

        {
            AABB box;

            box.SetMinMax(Vector3f(-1.0f,-1.0f,-1.0f),Vector3f(1.0f,1.0f,1.0f));        //这个不对，待查

            p->SetBoundingBox(box);
        }

        return p;
    }

    namespace
    {
        template<typename T>
        void CreateTorusIndices(PrimitiveCreater *pc,uint numberSlices,uint numberStacks)
        {
            IBTypeMap<T> ib_map(pc->GetIBMap());
            T *tp=ib_map;

            // loop counters
            uint sideCount, faceCount;

            // used to generate the indices
            uint v0, v1, v2, v3;

            for (sideCount = 0; sideCount < numberSlices; ++sideCount)
            {
                for (faceCount = 0; faceCount < numberStacks; ++faceCount)
                {
                    // get the number of the vertices for a face of the torus. They must be < numVertices
                    v0 =  ((sideCount       * (numberStacks + 1)) +  faceCount);
                    v1 = (((sideCount + 1)  * (numberStacks + 1)) +  faceCount);
                    v2 = (((sideCount + 1)  * (numberStacks + 1)) + (faceCount + 1));
                    v3 =  ((sideCount       * (numberStacks + 1)) + (faceCount + 1));

                    // first triangle of the face, counter clock wise winding
                    *tp = v0; ++tp;
                    *tp = v2; ++tp;
                    *tp = v1; ++tp;

                    // second triangle of the face, counter clock wise winding
                    *tp = v0; ++tp;
                    *tp = v3; ++tp;
                    *tp = v2; ++tp;
                }
            }
        }
    }//namespace

    Primitive *CreateTorus(PrimitiveCreater *pc,const TorusCreateInfo *tci)
    {
        if(!pc)return(nullptr);

        // s, t = parametric values of the equations, in the range [0,1]
        float s = 0;
        float t = 0;

        // sIncr, tIncr are increment values aplied to s and t on each loop iteration to generate the torus
        float sIncr;
        float tIncr;

        // to store precomputed sin and cos values
        float cos2PIs, sin2PIs, cos2PIt, sin2PIt;

        uint sideCount,faceCount;

        uint numberVertices;
        uint numberIndices;

        // used later to help us calculating tangents vectors
        float helpVector[3] = { 0.0f, 1.0f, 0.0f };
        float helpQuaternion[4];
        float helpMatrix[16];

        float torusRadius = (tci->outerRadius - tci->innerRadius) / 2.0f;
        float centerRadius = tci->outerRadius - torusRadius;

        numberVertices = (tci->numberStacks + 1) * (tci->numberSlices + 1);
        numberIndices = tci->numberStacks * tci->numberSlices * 2 * 3; // 2 triangles per face * 3 indices per triangle

        if (tci->numberSlices < 3 || tci->numberStacks < 3 || numberVertices > GLUS_MAX_VERTICES || numberIndices > GLUS_MAX_INDICES)
            return(nullptr);

        sIncr = 1.0f / (float) tci->numberSlices;
        tIncr = 1.0f / (float) tci->numberStacks;

        if(!pc->Init("Torus",numberVertices,numberIndices))
            return(nullptr);                
                
        VABMapFloat vertex   (pc->GetVABMap(VAN::Position),VF_V3F);
        VABMapFloat normal   (pc->GetVABMap(VAN::Normal),VF_V3F);
        VABMapFloat tangent  (pc->GetVABMap(VAN::Tangent),VF_V3F);
        VABMapFloat tex_coord(pc->GetVABMap(VAN::TexCoord),VF_V2F);

        float *vp=vertex;
        float *np=normal;
        float *tp=tangent;
        float *tcp=tex_coord;

        if(!vp)
            return(nullptr);

        // generate vertices and its attributes
        for (sideCount = 0; sideCount <= tci->numberSlices; ++sideCount, s += sIncr)
        {
            // precompute some values
            cos2PIs = cos(2.0f * HGL_PI * s);
            sin2PIs = sin(2.0f * HGL_PI * s);

            t = 0.0f;
            for (faceCount = 0; faceCount <= tci->numberStacks; ++faceCount, t += tIncr)
            {
                // precompute some values
                cos2PIt = cos(2.0f * HGL_PI * t);
                sin2PIt = sin(2.0f * HGL_PI * t);

                // generate vertex and stores it in the right position
                *vp = torusRadius * sin2PIt; ++vp;
                *vp =-(centerRadius + torusRadius * cos2PIt) * cos2PIs; ++vp;
                *vp = (centerRadius + torusRadius * cos2PIt) * sin2PIs; ++vp;

                if(np)
                {
                    // generate normal and stores it in the right position
                    // NOTE: cos (2PIx) = cos (x) and sin (2PIx) = sin (x) so, we can use this formula
                    //       normal = {cos(2PIs)cos(2PIt) , sin(2PIs)cos(2PIt) ,sin(2PIt)}
                    *np =  sin2PIt;             ++np;
                    *np = -cos2PIs * cos2PIt;   ++np;
                    *np =  sin2PIs * cos2PIt;   ++np;
                }

                if(tcp)
                {
                    // generate texture coordinates and stores it in the right position
                    *tcp = s*tci->uv_scale.x; ++tcp;
                    *tcp = t*tci->uv_scale.y; ++tcp;
                }

                if(tp)
                {
                    // tangent roughly along +theta on walls, radial on caps; will override per usage
                    glusQuaternionRotateRzf(helpQuaternion, 360.0f * s);
                    glusQuaternionGetMatrix4x4f(helpMatrix, helpQuaternion);

                    glusMatrix4x4MultiplyVector3f(tp, helpMatrix, helpVector);
                    tp+=3;
                }
            }
        }

        //索引
        {
            const IndexType index_type=pc->GetIndexType();

            if(index_type==IndexType::U16)CreateTorusIndices<uint16>(pc,tci->numberSlices,tci->numberStacks);else
            if(index_type==IndexType::U32)CreateTorusIndices<uint32>(pc,tci->numberSlices,tci->numberStacks);else
            if(index_type==IndexType::U8 )CreateTorusIndices<uint8 >(pc,tci->numberSlices,tci->numberStacks);else
                return(nullptr);
        }

        Primitive *p=pc->Create();

        {
            AABB aabb;

            // Calculate bounding box for the torus
            float maxExtent = centerRadius + torusRadius;
            float minExtent = centerRadius - torusRadius;

            aabb.SetMinMax(Vector3f(-torusRadius, -maxExtent, -maxExtent),
                           Vector3f( torusRadius,  maxExtent,  maxExtent));

            p->SetBoundingBox(aabb);
        }

        return p;
    }

    namespace
    {
        template<typename T>
        void CreateCylinderIndices(PrimitiveCreater *pc,const uint numberSlices)
        {
            IBTypeMap<T> ib_map(pc->GetIBMap());
            T *tp=ib_map;
            uint i;

            T centerIndex = 0;
            T indexCounter = 1;

            for (i = 0; i < numberSlices; i++)
            {
                *tp = centerIndex;      ++tp;
                *tp = indexCounter;     ++tp;
                *tp = indexCounter + 1; ++tp;

                indexCounter++;
            }
            indexCounter++;

            // Top
            centerIndex = indexCounter;
            indexCounter++;

            for (i = 0; i < numberSlices; i++)
            {
                *tp = centerIndex;      ++tp;
                *tp = indexCounter + 1; ++tp;
                *tp = indexCounter;     ++tp;

                indexCounter++;
            }
            indexCounter++;

            // Sides
            for (i = 0; i < numberSlices; i++)
            {
                *tp = indexCounter;     ++tp;
                *tp = indexCounter + 1; ++tp;
                *tp = indexCounter + 2; ++tp;

                *tp = indexCounter + 2; ++tp;
                *tp = indexCounter + 1; ++tp;
                *tp = indexCounter + 3; ++tp;

                indexCounter += 2;
            }
        }
    }//namespace

    Primitive *CreateCylinder(PrimitiveCreater *pc,const CylinderCreateInfo *cci)
    {
        uint numberIndices = cci->numberSlices * 3 * 2 + cci->numberSlices * 6;

        if(numberIndices<=0)
            return(nullptr);

        uint numberVertices = (cci->numberSlices + 2) * 2 + (cci->numberSlices + 1) * 2;

        if(!pc->Init("Cylinder",numberVertices,numberIndices))
            return(nullptr);

        float angleStep = (2.0f * HGL_PI) / ((float) cci->numberSlices);

        if (cci->numberSlices < 3 || numberVertices > GLUS_MAX_VERTICES || numberIndices > GLUS_MAX_INDICES)
            return nullptr;
                
        VABMapFloat vertex   (pc->GetVABMap(VAN::Position),VF_V3F);
        VABMapFloat normal   (pc->GetVABMap(VAN::Normal),VF_V3F);
        VABMapFloat tangent  (pc->GetVABMap(VAN::Tangent),VF_V3F);
        VABMapFloat tex_coord(pc->GetVABMap(VAN::TexCoord),VF_V2F);

        float *vp=vertex;
        float *np=normal;
        float *tp=tangent;
        float *tcp=tex_coord;

        if(!vp)
            return(nullptr);

        *vp =  0.0f;             ++vp;
        *vp =  0.0f;             ++vp;
        *vp = -cci->halfExtend;  ++vp;

        if(np)
        {
            *np = 0.0f; ++np;
            *np = 0.0f; ++np;
            *np =-1.0f; ++np;
        }

        if(tp)
        {
            *tp = 0.0f; ++tp;
            *tp = 1.0f; ++tp;
            *tp = 0.0f; ++tp;
        }

        if(tcp)
        {
            *tcp = 0.0f; ++tcp;
            *tcp = 0.0f; ++tcp;
        }

        for(uint i = 0; i < cci->numberSlices + 1; i++)
        {
            float currentAngle = angleStep * (float)i;

            *vp =  cos(currentAngle) * cci->radius; ++vp;
            *vp = -sin(currentAngle) * cci->radius; ++vp;
            *vp = -cci->halfExtend;                 ++vp;

            if(np)
            {
                *np = 0.0f; ++np;
                *np = 0.0f; ++np;
                *np =-1.0f; ++np;
            }

            if(tp)
            {
                *tp = sin(currentAngle);    ++tp;
                *tp = cos(currentAngle);    ++tp;
                *tp = 0.0f;                 ++tp;
            }

            if(tcp)
            {
                *tcp = 0.0f; ++tcp;
                *tcp = 0.0f; ++tcp;
            }
        }

        *vp = 0.0f;             ++vp;
        *vp = 0.0f;             ++vp;
        *vp = cci->halfExtend;  ++vp;

        if(np)
        {
            *np = 0.0f;    ++np;
            *np = 0.0f;    ++np;
            *np = 1.0f;    ++np;
        }

        if(tp)
        {
            *tp =  0.0f; ++tp;
            *tp = -1.0f; ++tp;
            *tp =  0.0f; ++tp;
        }

        if(tcp)
        {
            *tcp = 1.0f; ++tcp;
            *tcp = 1.0f; ++tcp;
        }

        for(uint i = 0; i < cci->numberSlices + 1; i++)
        {
            float currentAngle = angleStep * (float)i;

            *vp =  cos(currentAngle) * cci->radius; ++vp;
            *vp = -sin(currentAngle) * cci->radius; ++vp;
            *vp =  cci->halfExtend;                 ++vp;

            if(np)
            {
                *np = 0.0f;    ++np;
                *np = 0.0f;    ++np;
                *np = 1.0f;    ++np;
            }

            if(tp)
            {
                *tp = -sin(currentAngle);   ++tp;
                *tp = -cos(currentAngle);   ++tp;
                *tp = 0.0f;                 ++tp;
            }

            if(tcp)
            {
                *tcp = 1.0f; ++tcp;
                *tcp = 1.0f; ++tcp;
            }
        }

        for(uint i = 0; i < cci->numberSlices + 1; i++)
        {
            float currentAngle = angleStep * (float)i;

            float sign = -1.0f;

            for (uint j = 0; j < 2; j++)
            {
                *vp =  cos(currentAngle) * cci->radius;     ++vp;
                *vp = -sin(currentAngle) * cci->radius;     ++vp;
                *vp =  cci->halfExtend * sign;              ++vp;

                if(np)
                {
                    *np =  cos(currentAngle);   ++np;
                    *np = -sin(currentAngle);   ++np;
                    *np = 0.0f;                 ++np;
                }

                if(tp)
                {
                    *tp = -sin(currentAngle);   ++tp;
                    *tp = -cos(currentAngle);   ++tp;
                    *tp = 0.0f;                 ++tp;
                }

                if(tcp)
                {
                    *tcp = (float)i / (float)cci->numberSlices;  ++tcp;
                    *tcp = (sign + 1.0f) / 2.0f;                 ++tcp;
                }

                sign = 1.0f;
            }
        }

        //索引
        {
            const IndexType index_type=pc->GetIndexType();

            if(index_type==IndexType::U16)CreateCylinderIndices<uint16>(pc,cci->numberSlices);else
            if(index_type==IndexType::U32)CreateCylinderIndices<uint32>(pc,cci->numberSlices);else
            if(index_type==IndexType::U8 )CreateCylinderIndices<uint8 >(pc,cci->numberSlices);else
                return(nullptr);
        }

        Primitive *p=pc->Create();

        p->SetBoundingBox(  Vector3f(-cci->radius,-cci->radius,-cci->halfExtend),
                            Vector3f( cci->radius, cci->radius, cci->halfExtend));

        return p;
    }

    namespace
    {
        template<typename T>
        void CreateConeIndices(PrimitiveCreater *pc,const uint numberSlices,const uint numberStacks)
        {
            IBTypeMap<T> ib_map(pc->GetIBMap());
            T *tp=ib_map;

            // Bottom
            uint centerIndex = 0;
            uint indexCounter = 1;
            uint i,j;

            for (i = 0; i < numberSlices; i++)
            {
                *tp = centerIndex;      ++tp;
                *tp = indexCounter;     ++tp;
                *tp = indexCounter + 1; ++tp;

                indexCounter++;
            }
            indexCounter++;

            // Sides
            for (j = 0; j < numberStacks; ++j)
            {
                for (i = 0; i < numberSlices; ++i)
                {
                    *tp = indexCounter;                     ++tp;
                    *tp = indexCounter + numberSlices + 1;  ++tp;
                    *tp = indexCounter + 1;                 ++tp;

                    *tp = indexCounter + 1;                 ++tp;
                    *tp = indexCounter + numberSlices + 1;  ++tp;
                    *tp = indexCounter + numberSlices + 2;  ++tp;

                    indexCounter++;
                }
                indexCounter++;
            }
        }
    }//namespace

    Primitive *CreateCone(PrimitiveCreater *pc,const ConeCreateInfo *cci)
    {
        uint i, j;

        uint numberVertices = (cci->numberSlices + 2) + (cci->numberSlices + 1) * (cci->numberStacks + 1);
        uint numberIndices = cci->numberSlices * 3 + cci->numberSlices * 6 * cci->numberStacks;

        if(!pc->Init("Cone",numberVertices,numberIndices))
            return(nullptr);

        float angleStep = (2.0f * HGL_PI) / ((float) cci->numberSlices);

        float h = 2.0f * cci->halfExtend;
        float r = cci->radius;
        float l = sqrtf(h*h + r*r);

        if (cci->numberSlices < 3 || cci->numberStacks < 1 || numberVertices > GLUS_MAX_VERTICES || numberIndices > GLUS_MAX_INDICES)
            return nullptr;
                
        VABMapFloat vertex   (pc->GetVABMap(VAN::Position),VF_V3F);
        VABMapFloat normal   (pc->GetVABMap(VAN::Normal),VF_V3F);
        VABMapFloat tangent  (pc->GetVABMap(VAN::Tangent),VF_V3F);
        VABMapFloat tex_coord(pc->GetVABMap(VAN::TexCoord),VF_V2F);

        float *vp=vertex;
        float *np=normal;
        float *tp=tangent;
        float *tcp=tex_coord;

        if(!vp)
            return(nullptr);

        *vp =  0.0f;            ++vp;
        *vp =  0.0f;            ++vp;
        *vp = -cci->halfExtend; ++vp;

        if(np)
        {
            *np = 0.0f;++np;
            *np = 0.0f;++np;
            *np =-1.0f;++np;
        }

        if(tp)
        {
            *tp = 0.0f; ++tp;
            *tp = 1.0f; ++tp;
            *tp = 0.0f; ++tp;
        }

        if(tcp)
        {
            *tcp = 0.0f; ++tcp;
            *tcp = 0.0f; ++tcp;
        }

        for (i = 0; i < cci->numberSlices + 1; i++)
        {
            float currentAngle = angleStep * (float)i;

            *vp =  cos(currentAngle) * cci->radius; ++vp;
            *vp = -sin(currentAngle) * cci->radius; ++vp;
            *vp = -cci->halfExtend;                 ++vp;

            if(np)
            {
                *np = 0.0f;++np;
                *np = 0.0f;++np;
                *np =-1.0f;++np;
            }

            if(tp)
            {
                *tp = sin(currentAngle);    ++tp;
                *tp = cos(currentAngle);    ++tp;
                *tp = 0.0f;                 ++tp;
            }

            if(tcp)
            {
                *tcp = 0.0f; ++tcp;
                *tcp = 0.0f; ++tcp;
            }
        }

        for (j = 0; j < cci->numberStacks + 1; j++)
        {
            float level = (float)j / (float)cci->numberStacks;

            for (i = 0; i < cci->numberSlices + 1; i++)
            {
                float currentAngle = angleStep * (float)i;

                *vp =  cos(currentAngle) * cci->radius * (1.0f - level);    ++vp;
                *vp = -sin(currentAngle) * cci->radius * (1.0f - level);    ++vp;
                *vp = -cci->halfExtend + 2.0f * cci->halfExtend * level;    ++vp;

                if(np)
                {
                    *np = h / l *  cos(currentAngle);   ++np;
                    *np =-h / l *  sin(currentAngle);   ++np;
                    *np = r / l;                        ++np;
                }

                if(tp)
                {
                    *tp = -sin(currentAngle);   ++tp;
                    *tp = -cos(currentAngle);   ++tp;
                    *tp = 0.0f;                 ++tp;
                }

                if(tcp)
                {
                    *tcp = (float)i / (float)cci->numberSlices;  ++tcp;
                    *tcp = level;                                ++tcp;
                }
            }
        }

        //索引
        {
            const IndexType index_type=pc->GetIndexType();

            if(index_type==IndexType::U16)CreateConeIndices<uint16>(pc,cci->numberSlices,cci->numberStacks);else
            if(index_type==IndexType::U32)CreateConeIndices<uint32>(pc,cci->numberSlices,cci->numberStacks);else
            if(index_type==IndexType::U8 )CreateConeIndices<uint8 >(pc,cci->numberSlices,cci->numberStacks);else
                return(nullptr);
        }

        Primitive *p=pc->Create();

        p->SetBoundingBox(Vector3f(-cci->radius,-cci->radius,-cci->halfExtend),
                            Vector3f( cci->radius, cci->radius, cci->halfExtend));

        return p;
    }

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
        // Points of a cube.
        /*     4            5 */    const float points[]={  -0.5,-0.5, 0.5,     0.5,-0.5,0.5,   0.5,-0.5,-0.5,  -0.5,-0.5,-0.5,
        /*     *------------* */                            -0.5, 0.5, 0.5,     0.5, 0.5,0.5,   0.5, 0.5,-0.5,  -0.5, 0.5,-0.5};
        /*    /|           /| */
        /*  0/ |         1/ | */
        /*  *--+---------*  | */
        /*  |  |         |  | */
        /*  | 7|         | 6| */
        /*  |  *---------+--* */
        /*  | /          | /  */
        /*  |/          2|/   */
        /* 3*------------*    */

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

    /**
    * 创建一个正方形阵列，专门作于渲染地形
    */
    Primitive *CreateSqaureArray(PrimitiveCreater *pc,const uint row,const uint col)
    {
        if(!pc)return(nullptr);
        if(row<=0||col<=0)return(nullptr);
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

    Primitive *CreateHollowCylinder(PrimitiveCreater *pc,const HollowCylinderCreateInfo *hcci)
    {
        if(!pc||!hcci) return nullptr;

        if(hcci->innerRadius<=0)        //如果内半径为0，则退化为实心圆柱
        {
            CylinderCreateInfo cci;

            cci.halfExtend=hcci->halfExtend;
            cci.radius=hcci->outerRadius;
            cci.numberSlices=hcci->numberSlices;

            return CreateCylinder(pc,&cci);
        }

        const uint slices = (hcci->numberSlices<3)?3:hcci->numberSlices;
        float r_in = hcci->innerRadius;
        float r_out= hcci->outerRadius;
        if(r_in>r_out){ float tmp=r_in; r_in=r_out; r_out=tmp; }
        const float r0 = r_in;
        const float r1 = r_out;
        const float he = hcci->halfExtend;

        // Vertices count: walls (outer + inner) + caps (top + bottom)
        const uint wall_vert_per_side = (slices + 1) * 2; // for strip: (slices+1) columns, 2 rows (bottom/top)
        const uint cap_vert_per = (slices + 1) * 2;       // outer+inner per slice step

        const uint numberVertices = wall_vert_per_side * 2 + cap_vert_per * 2;
        const uint numberIndices = (slices * 2 /*two walls*/ + slices * 2 /*two caps*/) * 6; // each slice -> 2 tris -> 6 indices per triangle

        if(!pc->Init("HollowCylinder", numberVertices, numberIndices))
            return nullptr;

        VABMapFloat pos   (pc->GetVABMap(VAN::Position), VF_V3F);
        VABMapFloat nrm   (pc->GetVABMap(VAN::Normal),   VF_V3F);
        VABMapFloat tan   (pc->GetVABMap(VAN::Tangent),  VF_V3F);
        VABMapFloat uv    (pc->GetVABMap(VAN::TexCoord), VF_V2F);

        float *vp = pos;
        float *np = nrm;
        float *tp = tan;
        float *uvp= uv;

        const float dtheta = (2.0f * HGL_PI) / float(slices);

        auto write_vertex = [&](float x,float y,float z, float nx, float ny, float nz, float u, float v)
        {
            if(vp){ *vp++=x; *vp++=y; *vp++=z; }
            if(np){ *np++=nx; *np++=ny; *np++=nz; }
            if(tp){
                // 壁面：切线 = 法线绕Z旋90度；端盖：给稳定切线(1,0,0)
                if(fabsf(nz) > 0.5f){ *tp++ = 1.0f; *tp++ = 0.0f; *tp++ = 0.0f; }
                else                 { *tp++ = -ny;  *tp++ =  nx;  *tp++ = 0.0f; }
            }
            if(uvp){ *uvp++=u; *uvp++=v; }
        };

        // Track base indices
        const uint wall_outer_start = 0;
        auto write_wall = [&](float radius, bool outer)
        {
            for(uint i=0;i<=slices;i++)
            {
                float ang = dtheta * float(i);
                float cx = cos(ang), sy = -sin(ang); // 保持与 CreateCylinder 相同的参数化
                // wall normal: outward from surface
                float nx = outer? cx : -cx;
                float ny = outer? sy : -sy;
                float u = float(i) / float(slices);
                // bottom
                write_vertex(radius*cx, radius*sy, -he, nx, ny, 0.0f, u, 0.0f);
                // top
                write_vertex(radius*cx, radius*sy,  he, nx, ny, 0.0f, u, 1.0f);
            }
        };

        write_wall(r1, true);
        const uint wall_inner_start = wall_outer_start + wall_vert_per_side;
        write_wall(r0, false);

        auto write_cap = [&](float z, bool top)
        {
            for(uint i=0;i<=slices;i++)
            {
                float ang = dtheta * float(i);
                float cx = cos(ang), sy = -sin(ang);
                float nx = 0.0f, ny = 0.0f, nz = top? 1.0f : -1.0f;
                float u = (float(i) / float(slices)) * hcci->cap_angular_tiles;
                float v_outer = hcci->cap_radial_tiles; // at r1
                float v_inner = 0.0f;                   // at r0
                write_vertex(r1*cx, r1*sy, z, nx, ny, nz, u, v_outer);
                write_vertex(r0*cx, r0*sy, z, nx, ny, nz, u, v_inner);
            }
        };

        const uint cap_top_start = wall_inner_start + wall_vert_per_side;
        write_cap(+he, true);
        const uint cap_bottom_start = cap_top_start + cap_vert_per;
        write_cap(-he, false);

        // Indices
        IBMap *ib_map = pc->GetIBMap();
        const IndexType it = pc->GetIndexType();

        auto emit_wall_indices = [&](auto *ip, uint base, bool invert)
        {
            for(uint i=0;i<slices;i++)
            {
                uint v0 = base + i*2;
                uint v1 = base + i*2 + 1;
                uint v2 = base + (i+1)*2;
                uint v3 = base + (i+1)*2 + 1;
                if(!invert)
                {   // 外壁：与 Cylinder 一致（正面朝外）
                    *ip++ = (decltype(*ip))v0; *ip++ = (decltype(*ip))v1; *ip++ = (decltype(*ip))v2;
                    *ip++ = (decltype(*ip))v2; *ip++ = (decltype(*ip))v1; *ip++ = (decltype(*ip))v3;
                }
                else
                {   // 内壁：反向（正面朝内）
                    *ip++ = (decltype(*ip))v0; *ip++ = (decltype(*ip))v2; *ip++ = (decltype(*ip))v1;
                    *ip++ = (decltype(*ip))v2; *ip++ = (decltype(*ip))v3; *ip++ = (decltype(*ip))v1;
                }
            }
            return ip;
        };

        auto emit_cap_indices = [&](auto *ip, uint base, bool top)
        {
            for(uint i=0;i<slices;i++)
            {
                uint o0 = base + i*2;
                uint i0 = base + i*2 + 1;
                uint o1 = base + (i+1)*2;
                uint i1 = base + (i+1)*2 + 1;
                if(top)
                {   // 顶盖：法线+Z，俯视为正面
                    *ip++ = (decltype(*ip))o0; *ip++ = (decltype(*ip))i0; *ip++ = (decltype(*ip))o1;
                    *ip++ = (decltype(*ip))i0; *ip++ = (decltype(*ip))i1; *ip++ = (decltype(*ip))o1;
                }
                else
                {   // 底盖：法线-Z，从 -Z 看为正面
                    *ip++ = (decltype(*ip))o0; *ip++ = (decltype(*ip))o1; *ip++ = (decltype(*ip))i0;
                    *ip++ = (decltype(*ip))i0; *ip++ = (decltype(*ip))i1; *ip++ = (decltype(*ip))o1;
                }
            }
            return ip;
        };

        if(it==IndexType::U16)
        {
            IBTypeMap<uint16> im(ib_map);
            uint16 *ip = im;
            ip = emit_wall_indices(ip, wall_outer_start, false);
            ip = emit_wall_indices(ip, wall_inner_start, true);
            ip = emit_cap_indices(ip, cap_top_start, true);
            ip = emit_cap_indices(ip, cap_bottom_start, false);
        }
        else if(it==IndexType::U32)
        {
            IBTypeMap<uint32> im(ib_map);
            uint32 *ip = im;
            ip = emit_wall_indices(ip, wall_outer_start, false);
            ip = emit_wall_indices(ip, wall_inner_start, true);
            ip = emit_cap_indices(ip, cap_top_start, true);
            ip = emit_cap_indices(ip, cap_bottom_start, false);
        }
        else if(it==IndexType::U8)
        {
            IBTypeMap<uint8> im(ib_map);
            uint8 *ip = im;
            ip = emit_wall_indices(ip, wall_outer_start, false);
            ip = emit_wall_indices(ip, wall_inner_start, true);
            ip = emit_cap_indices(ip, cap_top_start, true);
            ip = emit_cap_indices(ip, cap_bottom_start, false);
        }
        else return nullptr;

        Primitive *p = pc->Create();
        if(p)
        {
            AABB aabb;
            aabb.SetMinMax(Vector3f(-r1,-r1,-he), Vector3f(r1,r1,he));
            p->SetBoundingBox(aabb);
        }
        return p;
    }

    Primitive *CreateHexSphere(PrimitiveCreater *pc,const HexSphereCreateInfo *hsci)
    {
        if(!pc||!hsci) return nullptr;

        // 生成基础二十面体
        struct Tri { uint a,b,c; };
        std::vector<Vector3f> verts;
        std::vector<Tri> tris;

        auto add = [&](float x,float y,float z){ verts.emplace_back(x,y,z); };

        const float t = (1.0f + sqrtf(5.0f)) * 0.5f; // golden ratio
        // 12 vertices of icosahedron
        add(-1,  t,  0); add( 1,  t,  0); add(-1, -t,  0); add( 1, -t,  0);
        add( 0, -1,  t); add( 0,  1,  t); add( 0, -1, -t); add( 0,  1, -t);
        add( t,  0, -1); add( t,  0,  1); add(-t,  0, -1); add(-t,  0,  1);

        for(auto &v:verts) v = glm::normalize(v);

        auto push = [&](uint a,uint b,uint c){ tris.push_back({a,b,c}); };
        // 20 faces; ensure Z-up and prefer clockwise front faces when looking from outside.
        push(0,11,5); push(0,5,1); push(0,1,7); push(0,7,10); push(0,10,11);
        push(1,5,9); push(5,11,4); push(11,10,2); push(10,7,6); push(7,1,8);
        push(3,9,4); push(3,4,2); push(3,2,6); push(3,6,8); push(3,8,9);
        push(4,9,5); push(2,4,11); push(6,2,10); push(8,6,7); push(9,8,1);

        // 索引缓存中间点
        struct EdgeKey { uint a,b; bool operator==(const EdgeKey& o)const{return a==o.a&&b==o.b;} };
        struct EdgeHash { size_t operator()(const EdgeKey& k)const { return (size_t(k.a)<<32) ^ k.b; } };
        std::unordered_map<EdgeKey,uint,EdgeHash> midpoint;

        auto get_mid = [&](uint a,uint b){
            EdgeKey key{std::min(a,b),std::max(a,b)};
            auto it = midpoint.find(key);
            if(it!=midpoint.end()) return it->second;
            Vector3f m = verts[a]+verts[b]; m = glm::normalize(m);
            uint id = (uint)verts.size(); verts.push_back(m); midpoint.emplace(key,id); return id;
        };

        // 细分
        for(uint s=0;s<hsci->subdivisions;s++)
        {
            std::vector<Tri> ntris; ntris.reserve(tris.size()*4);
            for(const auto &t : tris)
            {
                uint ab = get_mid(t.a,t.b);
                uint bc = get_mid(t.b,t.c);
                uint ca = get_mid(t.c,t.a);
                // 保持顺时针正面（a,b,c 为外观时顺时针），四分三角形
                ntris.push_back({t.a, ab, ca});
                ntris.push_back({t.b, bc, ab});
                ntris.push_back({t.c, ca, bc});
                ntris.push_back({ab, bc, ca});
            }
            tris.swap(ntris);
        }

        // 归一化到半径
        const float R = hsci->radius;
        for(auto &v:verts) v *= R;

        const uint vertex_count = (uint)verts.size();
        const uint index_count  = (uint)tris.size()*3;

        if(!pc->Init("HexSphere", vertex_count, index_count))
            return nullptr;

        VABMapFloat pos(pc->GetVABMap(VAN::Position), VF_V3F);
        VABMapFloat nrm(pc->GetVABMap(VAN::Normal),   VF_V3F);
        VABMapFloat tan(pc->GetVABMap(VAN::Tangent),  VF_V3F);
        VABMapFloat uv (pc->GetVABMap(VAN::TexCoord), VF_V2F);

        float *vp = pos;
        float *np = nrm;
        float *tp = tan;
        float *uvp= uv;

        if(!vp) return nullptr;

        // 写顶点属性：法线=单位方向，切线取经向方向（在极点退化时给固定值）
        for(const auto &v:verts)
        {
            *vp++ = v.x; *vp++ = v.y; *vp++ = v.z;

            if(np)
            {
                Vector3f n = glm::normalize(v);
                *np++ = n.x; *np++ = n.y; *np++ = n.z;
            }

            if(uvp)
            {
                Vector3f n = glm::normalize(v);
                // 球面 UV，经度[-pi,pi] -> u in [0,1]，纬度[-pi/2,pi/2] -> v in [0,1]
                float u = (atan2f(n.y, n.x) / (2.0f*HGL_PI)) + 0.5f;
                float vtex = (asinf(std::clamp(n.z, -1.0f, 1.0f))/HGL_PI) + 0.5f;
                *uvp++ = u * hsci->uv_scale.x;
                *uvp++ = vtex * hsci->uv_scale.y;
            }

            if(tp)
            {
                Vector3f n = glm::normalize(v);
                // 经向切线：沿 +theta（绕Z）方向近似：(-y, x, 0) 并去掉与 n 的投影
                Vector3f tdir(-n.y, n.x, 0.0f);
                if(glm::length(tdir)<1e-6f) tdir = Vector3f(1,0,0); // 极点备选
                tdir = (tdir - n * dot(n, tdir));
                tdir = glm::normalize(tdir);
                *tp++ = tdir.x; *tp++ = tdir.y; *tp++ = tdir.z;
            }
        }

        // 索引：顺时针为正面，直接按 tris 中的 (a,b,c) 顺序写
        {
            IBMap *ib = pc->GetIBMap();
            const IndexType it = pc->GetIndexType();
            if(it==IndexType::U16)
            {
                IBTypeMap<uint16> im(ib);
                uint16 *ip = im;
                for(const auto &t : tris){ *ip++=(uint16)t.a; *ip++=(uint16)t.b; *ip++=(uint16)t.c; }
            }
            else if(it==IndexType::U32)
            {
                IBTypeMap<uint32> im(ib);
                uint32 *ip = im;
                for(const auto &t : tris){ *ip++=t.a; *ip++=t.b; *ip++=t.c; }
            }
            else if(it==IndexType::U8)
            {
                IBTypeMap<uint8> im(ib);
                uint8 *ip = im;
                for(const auto &t : tris){ *ip++=(uint8)t.a; *ip++=(uint8)t.b; *ip++=(uint8)t.c; }
            }
            else return nullptr;
        }

        Primitive *p = pc->Create();
        if(p)
            p->SetBoundingBox(Vector3f(-R,-R,-R), Vector3f(R,R,R));
        return p;
    }

    // 辅助函数：计算向量叉积
    Vector3f CrossProduct(const Vector3f& a, const Vector3f& b) {
        return Vector3f(a.y * b.z - a.z * b.y,
                        a.z * b.x - a.x * b.z,
                        a.x * b.y - a.y * b.x);
    }

    // 辅助函数：向量标准化
    Vector3f Normalize(const Vector3f& v) {
        float len = sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        if (len > 0.0f) {
            return Vector3f(v.x / len, v.y / len, v.z / len);
        }
        return Vector3f(0, 0, 1);
    }

    // 辅助函数：构建局部坐标系
    void BuildLocalCoordinateSystem(const Vector3f& extrudeAxis, Vector3f& right, Vector3f& up) {
        Vector3f axis = Normalize(extrudeAxis);
        
        // 选择一个不与挤压轴平行的向量作为参考
        Vector3f ref = (fabs(axis.x) < 0.9f) ? Vector3f(1, 0, 0) : Vector3f(0, 1, 0);
        
        // 构建正交坐标系
        right = Normalize(CrossProduct(ref, axis));
        up = CrossProduct(axis, right);
    }

    Primitive *CreateExtrudedPolygon(PrimitiveCreater *pc, const ExtrudedPolygonCreateInfo *epci)
    {
        if (!pc || !epci || !epci->vertices || epci->vertexCount < 3) {
            return nullptr;
        }

        const uint vertexCount = epci->vertexCount;
        const bool generateCaps = epci->generateCaps;
        const bool generateSides = epci->generateSides;
        
        // 计算顶点和索引数量
        uint totalVertices = 0;
        uint totalIndices = 0;
        
        if (generateSides) {
            totalVertices += vertexCount * 2;  // 侧面顶点（每个原始顶点对应上下两个3D顶点）
            totalIndices += vertexCount * 6;   // 每个侧面四边形用2个三角形，6个索引
        }
        
        if (generateCaps) {
            totalVertices += vertexCount * 2;  // 顶底面顶点
            totalIndices += (vertexCount - 2) * 6;  // 每个面用三角扇形分解：(n-2)个三角形 * 2个面 * 3个索引
        }

        if (!pc->Init("ExtrudedPolygon", totalVertices, totalIndices, IndexType::U32)) {
            return nullptr;
        }

        VABMapFloat vertex_map(pc->GetVABMap(VAN::Position), VF_V3F);
        VABMapFloat normal_map(pc->GetVABMap(VAN::Normal), VF_V3F);
        
        if (!vertex_map) return nullptr;

        float *vp = vertex_map;
        float *np = normal_map;

        // 构建局部坐标系
        Vector3f right, up;
        BuildLocalCoordinateSystem(epci->extrudeAxis, right, up);
        Vector3f forward = Normalize(epci->extrudeAxis);

        // 计算挤压的起始和结束位置
        Vector3f extrudeOffset = forward * epci->extrudeDistance;

        uint vertexIndex = 0;
        
        // 生成侧面顶点
        if (generateSides) {
            for (uint i = 0; i < vertexCount; i++) {
                const Vector2f& v2d = epci->vertices[i];
                
                // 将2D顶点转换为3D世界坐标
                Vector3f v3d = right * v2d.x + up * v2d.y;
                
                // 底面顶点
                if (vp) {
                    *vp++ = v3d.x;
                    *vp++ = v3d.y;
                    *vp++ = v3d.z;
                }
                
                // 顶面顶点
                Vector3f topVertex = v3d + extrudeOffset;
                if (vp) {
                    *vp++ = topVertex.x;
                    *vp++ = topVertex.y;
                    *vp++ = topVertex.z;
                }
                
                // 计算侧面法线
                if (np) {
                    // 计算边的方向
                    uint nextIndex = (i + 1) % vertexCount;
                    Vector2f edge2d = epci->vertices[nextIndex] - v2d;
                    Vector3f edge3d = right * edge2d.x + up * edge2d.y;
                    
                    // 侧面法线 = edge × extrudeAxis
                    Vector3f sideNormal = Normalize(CrossProduct(edge3d, forward));
                    
                    // 确保法线指向外侧
                    if (!epci->clockwiseFront) {
                        sideNormal = sideNormal * -1.0f;
                    }
                    
                    // 底面顶点法线
                    *np++ = sideNormal.x;
                    *np++ = sideNormal.y;
                    *np++ = sideNormal.z;
                    
                    // 顶面顶点法线
                    *np++ = sideNormal.x;
                    *np++ = sideNormal.y;
                    *np++ = sideNormal.z;
                }
                
                vertexIndex += 2;
            }
        }
        
        // 生成顶底面顶点
        if (generateCaps) {
            // 底面顶点
            for (uint i = 0; i < vertexCount; i++) {
                const Vector2f& v2d = epci->vertices[i];
                Vector3f v3d = right * v2d.x + up * v2d.y;
                
                if (vp) {
                    *vp++ = v3d.x;
                    *vp++ = v3d.y;
                    *vp++ = v3d.z;
                }
                
                if (np) {
                    Vector3f bottomNormal = forward * -1.0f;
                    if (!epci->clockwiseFront) {
                        bottomNormal = bottomNormal * -1.0f;
                    }
                    *np++ = bottomNormal.x;
                    *np++ = bottomNormal.y;
                    *np++ = bottomNormal.z;
                }
            }
            
            // 顶面顶点
            for (uint i = 0; i < vertexCount; i++) {
                const Vector2f& v2d = epci->vertices[i];
                Vector3f v3d = right * v2d.x + up * v2d.y;
                Vector3f topVertex = v3d + extrudeOffset;
                
                if (vp) {
                    *vp++ = topVertex.x;
                    *vp++ = topVertex.y;
                    *vp++ = topVertex.z;
                }
                
                if (np) {
                    Vector3f topNormal = forward;
                    if (!epci->clockwiseFront) {
                        topNormal = topNormal * -1.0f;
                    }
                    *np++ = topNormal.x;
                    *np++ = topNormal.y;
                    *np++ = topNormal.z;
                }
            }
        }

        // 生成索引
        IBTypeMap<uint32> ib_map(pc->GetIBMap());
        uint32 *ip = ib_map;

        uint indexOffset = 0;
        
        // 生成侧面索引
        if (generateSides) {
            for (uint i = 0; i < vertexCount; i++) {
                uint next = (i + 1) % vertexCount;
                
                uint i0 = indexOffset + i * 2;      // 当前底面顶点
                uint i1 = indexOffset + i * 2 + 1;  // 当前顶面顶点
                uint i2 = indexOffset + next * 2;   // 下一个底面顶点
                uint i3 = indexOffset + next * 2 + 1; // 下一个顶面顶点
                
                if (epci->clockwiseFront) {
                    // 第一个三角形 (底面 -> 顶面 -> 下一个底面)
                    *ip++ = i0; *ip++ = i1; *ip++ = i2;
                    // 第二个三角形 (下一个底面 -> 顶面 -> 下一个顶面) 
                    *ip++ = i2; *ip++ = i1; *ip++ = i3;
                } else {
                    // 逆时针顶点顺序
                    *ip++ = i0; *ip++ = i2; *ip++ = i1;
                    *ip++ = i2; *ip++ = i3; *ip++ = i1;
                }
            }
            indexOffset += vertexCount * 2;
        }
        
        // 生成顶底面索引
        if (generateCaps) {
            // 底面索引（三角扇形）
            uint bottomStart = indexOffset;
            for (uint i = 1; i < vertexCount - 1; i++) {
                if (epci->clockwiseFront) {
                    *ip++ = bottomStart;
                    *ip++ = bottomStart + i + 1;
                    *ip++ = bottomStart + i;
                } else {
                    *ip++ = bottomStart;
                    *ip++ = bottomStart + i;
                    *ip++ = bottomStart + i + 1;
                }
            }
            
            // 顶面索引（三角扇形）
            uint topStart = indexOffset + vertexCount;
            for (uint i = 1; i < vertexCount - 1; i++) {
                if (epci->clockwiseFront) {
                    *ip++ = topStart;
                    *ip++ = topStart + i;
                    *ip++ = topStart + i + 1;
                } else {
                    *ip++ = topStart;
                    *ip++ = topStart + i + 1;
                    *ip++ = topStart + i;
                }
            }
        }

        Primitive *p = pc->Create();
        
        if (p) {
            // 计算包围盒
            Vector3f minBounds(FLT_MAX, FLT_MAX, FLT_MAX);
            Vector3f maxBounds(-FLT_MAX, -FLT_MAX, -FLT_MAX);
            
            for (uint i = 0; i < vertexCount; i++) {
                const Vector2f& v2d = epci->vertices[i];
                Vector3f v3d = right * v2d.x + up * v2d.y;
                
                // 底面顶点
                minBounds.x = std::min(minBounds.x, v3d.x);
                minBounds.y = std::min(minBounds.y, v3d.y);
                minBounds.z = std::min(minBounds.z, v3d.z);
                maxBounds.x = std::max(maxBounds.x, v3d.x);
                maxBounds.y = std::max(maxBounds.y, v3d.y);
                maxBounds.z = std::max(maxBounds.z, v3d.z);
                
                // 顶面顶点
                Vector3f topVertex = v3d + extrudeOffset;
                minBounds.x = std::min(minBounds.x, topVertex.x);
                minBounds.y = std::min(minBounds.y, topVertex.y);
                minBounds.z = std::min(minBounds.z, topVertex.z);
                maxBounds.x = std::max(maxBounds.x, topVertex.x);
                maxBounds.y = std::max(maxBounds.y, topVertex.y);
                maxBounds.z = std::max(maxBounds.z, topVertex.z);
            }
            
            p->SetBoundingBox(minBounds, maxBounds);
        }
        
        return p;
    }

    Primitive *CreateExtrudedRectangle(PrimitiveCreater *pc, float width, float height, float depth, const Vector3f &extrudeAxis)
    {
        // 创建矩形顶点（中心在原点）
        Vector2f rectVertices[4] = {
            {-width * 0.5f, -height * 0.5f},  // 左下
            { width * 0.5f, -height * 0.5f},  // 右下  
            { width * 0.5f,  height * 0.5f},  // 右上
            {-width * 0.5f,  height * 0.5f}   // 左上
        };

        ExtrudedPolygonCreateInfo epci;
        epci.vertices = rectVertices;
        epci.vertexCount = 4;
        epci.extrudeDistance = depth;
        epci.extrudeAxis = extrudeAxis;
        epci.generateCaps = true;
        epci.generateSides = true;
        epci.clockwiseFront = true;

        return CreateExtrudedPolygon(pc, &epci);
    }

    Primitive *CreateExtrudedCircle(PrimitiveCreater *pc, float radius, float height, uint segments, const Vector3f &extrudeAxis)
    {
        if (segments < 3) segments = 3;

        // 创建圆形顶点
        std::vector<Vector2f> circleVertices(segments);
        float angleStep = 2.0f * M_PI / segments;

        for (uint i = 0; i < segments; i++) {
            float angle = i * angleStep;
            circleVertices[i].x = cos(angle) * radius;
            circleVertices[i].y = sin(angle) * radius;
        }

        ExtrudedPolygonCreateInfo epci;
        epci.vertices = circleVertices.data();
        epci.vertexCount = segments;
        epci.extrudeDistance = height;
        epci.extrudeAxis = extrudeAxis;
        epci.generateCaps = true;
        epci.generateSides = true;
        epci.clockwiseFront = true;

        return CreateExtrudedPolygon(pc, &epci);
    }
}
