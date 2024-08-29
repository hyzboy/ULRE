// sphere、cylinear、cone、tours code from McNopper,website: https://github.com/McNopper/GLUS
// GL to VK: swap Y/Z of position/normal/tangent/index

#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/VertexAttribDataAccess.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKShaderModule.h>
#include<hgl/graph/PrimitiveCreater.h>

namespace hgl
{
    namespace graph
    {
        namespace inline_geometry
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

            Primitive *CreateCircle(PrimitiveCreater *pc,const CircleCreateInfo *cci)
            {
                if(!pc)return(nullptr);

                uint edge;
                uint vertex_count;

                if(cci->has_color)
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

                if(cci->has_color)
                {
                    if(!color.IsValid())
                        return(nullptr);

                    vertex->Write(cci->center);
                    color->Write(cci->center_color);
                }

                for(uint i=0;i<edge;i++)
                {
                    float ang=float(i)/float(cci->field_count)*360.0f;

                    float x=cci->center.x+sin(deg2rad(ang))*cci->radius.x;
                    float y=cci->center.y+cos(deg2rad(ang))*cci->radius.y;

                    vertex->Write(x,y);
                
                    if(cci->has_color)
                        color->Write(cci->border_color);
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

                return pc->Create();
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
                 * 注：cubemap纹理坐标系依然遵循OpenGL时代定下的坐标系，所以这里的position虽然使用vulkan坐标系，但在shader中当做cubemap纹理坐标使用时，需要在shader中转换为opengl坐标系(交换yz即可)
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

                //pc->CreateIBO16(6*2*3,indices);
                pc->WriteIBO(indices);
                return pc->Create();
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

                return pc->Create();
            }

            Primitive *CreateDome(PrimitiveCreater *pc,const uint numberSlices)
            {
                if(!pc)return(nullptr);

                uint i, j;

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

                for (i = 0; i < numberParallels + 1; i++)
                {
                    for (j = 0; j < numberSlices + 1; j++)
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

                return pc->Create();
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
                            // use quaternion to get the tangent vector
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
                return pc->Create();
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

                return pc->Create();
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
                    for (j = 0; j < numberStacks; j++)
                    {
                        for (i = 0; i < numberSlices; i++)
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

                return pc->Create();
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

                return pc->Create();
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

                const uint16 indices[]=
                {
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

                return pc->Create();
            }
        }//namespace inline_geometry
    }//namespace graph
}//namespace hgl
