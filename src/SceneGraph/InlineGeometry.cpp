// sphere、cylinear、cone、tours code from McNopper,website: https://github.com/McNopper/GLUS
// GL to VK: swap Y/Z of position/normal/tangent/index

#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/VertexAttribDataAccess.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKShaderModule.h>
#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/PrimitiveCreater.h>

namespace hgl
{
    namespace graph
    {
        namespace inline_geometry
        {
            Primitive *CreateRectangle(RenderResource *db,const VIL *vil,const RectangleCreateInfo *rci)
            {
                PrimitiveCreater rc(db,vil);

                if(!rc.Init(4))
                    return(nullptr);

                AutoDelete<VB2f> vertex=rc.AccessVAD<VB2f>(VAN::Position);

                if(!vertex)
                    return(nullptr);

                vertex->WriteRectFan(rci->scope);

                return rc.Finish();
            }

            Primitive *CreateGBufferCompositionRectangle(RenderResource *db,const VIL *vil)
            {
                RectangleCreateInfo rci;

                rci.scope.Set(-1,-1,2,2);

                return CreateRectangle(db,vil,&rci);
            }

            Primitive *CreateRoundRectangle(RenderResource *db,const VIL *vil,const RoundRectangleCreateInfo *rci)
            {
                PrimitiveCreater rc(db,vil);

                if(rci->radius==0||rci->round_per<=1)      //这是要画矩形
                {
                    if(!rc.Init(4))
                        return(nullptr);
                    
                    AutoDelete<VB2f> vertex=rc.AccessVAD<VB2f>(VAN::Position);

                    vertex->WriteRectFan(rci->scope);
                }
                else
                {
                    float radius=rci->radius;

                    if(radius>rci->scope.GetWidth()/2.0f)radius=rci->scope.GetWidth()/2.0f;
                    if(radius>rci->scope.GetHeight()/2.0f)radius=rci->scope.GetHeight()/2.0f;

                    if(!rc.Init(rci->round_per*4))
                        return(nullptr);
                
                    AutoDelete<VB2f> vertex=rc.AccessVAD<VB2f>(VAN::Position);

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

                return rc.Finish();
            }

            Primitive *CreateCircle(RenderResource *db,const VIL *vil,const CircleCreateInfo *cci)
            {
                PrimitiveCreater rc(db,vil);

                uint edge;

                if(cci->has_color)
                {
                    edge=cci->field_count+1;
                    if(!rc.Init(cci->field_count+2))return(nullptr);
                }
                else
                {
                    edge=cci->field_count;
                    if(!rc.Init(cci->field_count))return(nullptr);
                }

                AutoDelete<VB2f> vertex=rc.AccessVAD<VB2f>(VAN::Position);
                AutoDelete<VB4f> color=rc.AccessVAD<VB4f>(VAN::Color);

                if(!vertex)
                    return(nullptr);

                if(cci->has_color)
                {
                    if(!color)
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

                return rc.Finish();
            }

            Primitive *CreatePlaneGrid(RenderResource *db,const VIL *vil,const PlaneGridCreateInfo *pgci)
            {
                PrimitiveCreater rc(db,vil);

                if(!rc.Init(((pgci->step.x+1)+(pgci->step.y+1))*2))
                    return(nullptr);

                AutoDelete<VB3f> vertex=rc.AccessVAD<VB3f>(VAN::Position);
                for(uint row=0;row<=pgci->step.x;row++)
                {
                    float pos=float(row)/float(pgci->step.x);

                    vertex->WriteLine(  to(pgci->coord[0],pgci->coord[1],pos),
                                        to(pgci->coord[3],pgci->coord[2],pos));
                }

                for(uint col=0;col<=pgci->step.y;col++)
                {
                    float pos=float(col)/float(pgci->step.y);

                    vertex->WriteLine(to(pgci->coord[1],pgci->coord[2],pos),
                                      to(pgci->coord[0],pgci->coord[3],pos));
                }

                AutoDelete<VB1f> lum=rc.AccessVAD<VB1f>(VAN::Luminance);
                if(lum)
                {
                    for(uint row=0;row<=pgci->step.x;row++)
                    {
                        if((row%pgci->side_step.x)==0)
                            lum->RepeatWrite(pgci->side_lum,2);
                        else
                            lum->RepeatWrite(pgci->lum,2);
                    }

                    for(uint col=0;col<=pgci->step.y;col++)
                    {
                        if((col%pgci->side_step.y)==0)
                            lum->RepeatWrite(pgci->side_lum,2);
                        else
                            lum->RepeatWrite(pgci->lum,2);
                    }
                }

                return rc.Finish();
            }

            Primitive *CreatePlane(RenderResource *db,const VIL *vil,const PlaneCreateInfo *pci)
            {
                const   float       xy_vertices [] = { -0.5f,-0.5f,0.0f,  +0.5f,-0.5f,0.0f,    +0.5f,+0.5f,0.0f,    -0.5f,+0.5f,0.0f   };
                        float       xy_tex_coord[] = {  0.0f, 0.0f,        1.0f, 0.0f,          1.0f, 1.0f,          0.0f, 1.0f        };
                const   Vector3f    xy_normal(0.0f,0.0f,1.0f);
                const   Vector3f    xy_tangent(1.0f,0.0f,0.0f);

                PrimitiveCreater rc(db,vil);

                if(!rc.Init(4))
                    return(nullptr);
            
                rc.WriteVAD(VAN::Position,xy_vertices,sizeof(xy_vertices));

                {
                    AutoDelete<VB3f> normal=rc.AccessVAD<VB3f>(VAN::Normal);

                    if(normal)normal->RepeatWrite(xy_normal,4);
                }

                {
                    AutoDelete<VB3f> tangent=rc.AccessVAD<VB3f>(VAN::Tangent);

                    tangent->RepeatWrite(xy_tangent,4);
                }

                {
                    AutoDelete<VB2f> tex_coord=rc.AccessVAD<VB2f>(VAN::TexCoord);

                    if(tex_coord)
                    {
                        xy_tex_coord[2]=xy_tex_coord[4]=pci->tile.x;
                        xy_tex_coord[5]=xy_tex_coord[7]=pci->tile.y;

                        tex_coord->Write(xy_tex_coord);
                    }
                }

                return rc.Finish();
            }

            Primitive *CreateCube(RenderResource *db,const VIL *vil,const CubeCreateInfo *cci)
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

                PrimitiveCreater rc(db,vil);

                if(!rc.Init(24))
                    return(nullptr);

                rc.WriteVAD(VAN::Position,positions,sizeof(positions));

                if(cci->normal)
                rc.WriteVAD(VAN::Normal,normals,sizeof(normals));

                if(cci->tangent)
                rc.WriteVAD(VAN::Tangent,tangents,sizeof(tangents));

                if(cci->tex_coord)
                rc.WriteVAD(VAN::TexCoord,tex_coords,sizeof(tex_coords));

                if(cci->color_type!=CubeCreateInfo::ColorType::NoColor)
                {                
                    RANGE_CHECK_RETURN_NULLPTR(cci->color_type);

                    AutoDelete<VB4f> color=rc.AccessVAD<VB4f>(VAN::Color);

                    if(color)
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

                rc.CreateIBO16(6*2*3,indices);
                return rc.Finish();
            }

            template<typename T> void CreateSphereIndices(T *tp,uint numberParallels,const uint numberSlices)
            {
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
            Primitive *CreateSphere(RenderResource *db,const VIL *vil,const uint numberSlices)
            {
                PrimitiveCreater rc(db,vil);

                uint numberParallels = (numberSlices+1) / 2;
                uint numberVertices = (numberParallels + 1) * (numberSlices + 1);
                uint numberIndices = numberParallels * numberSlices * 6;

                const double angleStep = double(2.0f * HGL_PI) / ((double) numberSlices);

                // used later to help us calculating tangents vectors
                float helpVector[3] = { 1.0f, 0.0f, 0.0f };
                float helpQuaternion[4];
                float helpMatrix[16];
                float tex_x;

                if(!rc.Init(numberVertices))
                    return(nullptr);

                AutoDelete<VB3f> vertex=rc.AccessVAD<VB3f>(VAN::Position);
                AutoDelete<VB3f> normal=rc.AccessVAD<VB3f>(VAN::Normal);
                AutoDelete<VB3f> tangent=rc.AccessVAD<VB3f>(VAN::Tangent);
                AutoDelete<VB2f> tex_coord=rc.AccessVAD<VB2f>(VAN::TexCoord);

                float *vp=vertex->Get();
                float *np=normal?normal->Get():nullptr;
                float *tp=tangent?tangent->Get():nullptr;
                float *tcp=tex_coord?tex_coord->Get():nullptr;

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

                if(numberVertices<=0xffff)
                    CreateSphereIndices<uint16>(rc.CreateIBO16(numberIndices),numberParallels,numberSlices);
                else
                    CreateSphereIndices<uint32>(rc.CreateIBO32(numberIndices),numberParallels,numberSlices);

                return rc.Finish();
            }

            Primitive *CreateDome(RenderResource *db,const VIL *vil,const uint numberSlices)
            {
                PrimitiveCreater rc(db,vil);

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

                if(!rc.Init(numberVertices))
                    return(nullptr);

                AutoDelete<VB3f> vertex=rc.AccessVAD<VB3f>(VAN::Position);
                AutoDelete<VB3f> normal=rc.AccessVAD<VB3f>(VAN::Normal);
                AutoDelete<VB3f> tangent=rc.AccessVAD<VB3f>(VAN::Tangent);
                AutoDelete<VB2f> tex_coord=rc.AccessVAD<VB2f>(VAN::TexCoord);
            
                float *vp=vertex->Get();
                float *np=normal?normal->Get():nullptr;
                float *tp=tangent?tangent->Get():nullptr;
                float *tcp=tex_coord?tex_coord->Get():nullptr;

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

                if(numberVertices<=0xffff)
                    CreateSphereIndices<uint16>(rc.CreateIBO16(numberIndices),numberParallels,numberSlices);
                else
                    CreateSphereIndices<uint32>(rc.CreateIBO32(numberIndices),numberParallels,numberSlices);

                return rc.Finish();
            }

            namespace
            {
                template<typename T>
                void CreateTorusIndices(T *tp,uint numberSlices,uint numberStacks)
                {
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

            Primitive *CreateTorus(RenderResource *db,const VIL *vil,const TorusCreateInfo *tci)
            {
                PrimitiveCreater rc(db,vil);

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

                if(!rc.Init(numberVertices))
                    return(nullptr);

                AutoDelete<VB3f> vertex=rc.AccessVAD<VB3f>(VAN::Position);
                AutoDelete<VB3f> normal=rc.AccessVAD<VB3f>(VAN::Normal);
                AutoDelete<VB3f> tangent=rc.AccessVAD<VB3f>(VAN::Tangent);
                AutoDelete<VB2f> tex_coord=rc.AccessVAD<VB2f>(VAN::TexCoord);

                float *vp=vertex->Get();
                float *np=normal?normal->Get():nullptr;
                float *tp=tangent?tangent->Get():nullptr;
                float *tcp=tex_coord?tex_coord->Get():nullptr;

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
                        *vp = (centerRadius + torusRadius * cos2PIt) * cos2PIs; ++vp;
                        *vp = torusRadius * sin2PIt; ++vp;
                        *vp = (centerRadius + torusRadius * cos2PIt) * sin2PIs; ++vp;

                        if(np)
                        {
                            // generate normal and stores it in the right position
                            // NOTE: cos (2PIx) = cos (x) and sin (2PIx) = sin (x) so, we can use this formula
                            //       normal = {cos(2PIs)cos(2PIt) , sin(2PIs)cos(2PIt) ,sin(2PIt)}
                            *np = +cos2PIs * cos2PIt;   ++np;
                            *np = -sin2PIt;             ++np;
                            *np = +sin2PIs * cos2PIt;   ++np;
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

                if(numberVertices<=0xffff)
                    CreateTorusIndices<uint16>(rc.CreateIBO16(numberIndices),tci->numberSlices,tci->numberStacks);
                else
                    CreateTorusIndices<uint32>(rc.CreateIBO32(numberIndices),tci->numberSlices,tci->numberStacks);

                return rc.Finish();
            }

            namespace
            {
                template<typename T>
                void CreateCylinderIndices(T *tp,const uint numberSlices)
                {
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

            Primitive *CreateCylinder(RenderResource *db,const VIL *vil,const CylinderCreateInfo *cci)
            {
                uint numberIndices = cci->numberSlices * 3 * 2 + cci->numberSlices * 6;

                if(numberIndices<=0)
                    return(nullptr);

                PrimitiveCreater rc(db,vil);

                uint numberVertices = (cci->numberSlices + 2) * 2 + (cci->numberSlices + 1) * 2;

                if(!rc.Init(numberVertices))
                    return(nullptr);

                float angleStep = (2.0f * HGL_PI) / ((float) cci->numberSlices);

                if (cci->numberSlices < 3 || numberVertices > GLUS_MAX_VERTICES || numberIndices > GLUS_MAX_INDICES)
                    return nullptr;

                AutoDelete<VB3f> vertex=rc.AccessVAD<VB3f>(VAN::Position);
                AutoDelete<VB3f> normal=rc.AccessVAD<VB3f>(VAN::Normal);
                AutoDelete<VB3f> tangent=rc.AccessVAD<VB3f>(VAN::Tangent);
                AutoDelete<VB2f> tex_coord=rc.AccessVAD<VB2f>(VAN::TexCoord);

                float *vp=vertex->Get();
                float *np=normal?normal->Get():nullptr;
                float *tp=tangent?tangent->Get():nullptr;
                float *tcp=tex_coord?tex_coord->Get():nullptr;

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
                            *np =  sin(currentAngle);   ++np;
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

                if(numberVertices<=0xffff)
                    CreateCylinderIndices<uint16>(rc.CreateIBO16(numberIndices),cci->numberSlices);
                else
                    CreateCylinderIndices<uint32>(rc.CreateIBO32(numberIndices),cci->numberSlices);

                return rc.Finish();
            }

            namespace
            {
                template<typename T>
                void CreateConeIndices(T *tp,const uint numberSlices,const uint numberStacks)
                {
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

            Primitive *CreateCone(RenderResource *db,const VIL *vil,const ConeCreateInfo *cci)
            {
                PrimitiveCreater rc(db,vil);

                uint i, j;

                uint numberVertices = (cci->numberSlices + 2) + (cci->numberSlices + 1) * (cci->numberStacks + 1);

                if(!rc.Init(numberVertices))
                    return(nullptr);

                uint numberIndices = cci->numberSlices * 3 + cci->numberSlices * 6 * cci->numberStacks;

                float angleStep = (2.0f * HGL_PI) / ((float) cci->numberSlices);

                float h = 2.0f * cci->halfExtend;
                float r = cci->radius;
                float l = sqrtf(h*h + r*r);

                if (cci->numberSlices < 3 || cci->numberStacks < 1 || numberVertices > GLUS_MAX_VERTICES || numberIndices > GLUS_MAX_INDICES)
                    return nullptr;

                AutoDelete<VB3f> vertex=rc.AccessVAD<VB3f>(VAN::Position);
                AutoDelete<VB3f> normal=rc.AccessVAD<VB3f>(VAN::Normal);
                AutoDelete<VB3f> tangent=rc.AccessVAD<VB3f>(VAN::Tangent);
                AutoDelete<VB2f> tex_coord=rc.AccessVAD<VB2f>(VAN::TexCoord);

                float *vp=vertex->Get();
                float *np=normal?normal->Get():nullptr;
                float *tp=tangent?tangent->Get():nullptr;
                float *tcp=tex_coord?tex_coord->Get():nullptr;

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
                            *np = h / l *  sin(currentAngle);   ++np;
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

                if(numberVertices<=0xffff)
                    CreateConeIndices<uint16>(rc.CreateIBO16(numberIndices),cci->numberSlices,cci->numberStacks);
                else
                    CreateConeIndices<uint32>(rc.CreateIBO32(numberIndices),cci->numberSlices,cci->numberStacks);

                return rc.Finish();
            }

            Primitive *CreateAxis(RenderResource *db,const VIL *vil,const AxisCreateInfo *aci)
            {
                if(!db||!vil||!aci)return(nullptr);

                PrimitiveCreater rc(db,vil);

                if(!rc.Init(6))
                    return(nullptr);

                AutoDelete<VB3f> vertex=rc.AccessVAD<VB3f>(VAN::Position);
                AutoDelete<VB4f> color=rc.AccessVAD<VB4f>(VAN::Color);

                if(!vertex||!color)
                    return(nullptr);

                const float s=aci->size;

                vertex->Write(0,0,0);color->Write(aci->color[0]);
                vertex->Write(s,0,0);color->Write(aci->color[0]);
                vertex->Write(0,0,0);color->Write(aci->color[1]);
                vertex->Write(0,s,0);color->Write(aci->color[1]);
                vertex->Write(0,0,0);color->Write(aci->color[2]);
                vertex->Write(0,0,s);color->Write(aci->color[2]);

                return rc.Finish();
            }

            Primitive *CreateBoundingBox(RenderResource *db,const VIL *vil,const BoundingBoxCreateInfo *cci)
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

                PrimitiveCreater rc(db,vil);

                if(!rc.Init(8))
                    return(nullptr);

                AutoDelete<VB3f> vertex=rc.AccessVAD<VB3f>(VAN::Position);

                if(!vertex)return(nullptr);

                rc.WriteVAD(VAN::Position,points,sizeof(points));

                if(cci->color_type!=BoundingBoxCreateInfo::ColorType::NoColor)
                {
                    RANGE_CHECK_RETURN_NULLPTR(cci->color_type);

                    AutoDelete<VB4f> color=rc.AccessVAD<VB4f>(VAN::Color);

                    if(color)
                    {
                        if(cci->color_type==BoundingBoxCreateInfo::ColorType::SameColor)
                            color->RepeatWrite(cci->color[0],8);
                        else
                        if(cci->color_type==BoundingBoxCreateInfo::ColorType::VertexColor)
                            color->Write(cci->color,8);
                    }
                }

                rc.CreateIBO16(24,indices);

                return rc.Finish();
            }
        }//namespace inline_geometry
    }//namespace graph
}//namespace hgl
