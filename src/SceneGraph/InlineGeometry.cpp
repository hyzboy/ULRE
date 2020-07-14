// sphere、cylinear、cone、tours code from McNopper,website: https://github.com/McNopper/GLUS
// GL to VK: swap Y/Z of position/normal/tangent/index

#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/VertexAttribBuffer.h>
#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/graph/vulkan/VKShaderModule.h>
#include<hgl/graph/SceneDB.h>
namespace hgl
{
    namespace graph
    {
        namespace
        {
            template<typename VERTEX_VB_FORMAT>
            struct GeometryCreater
            {
                SceneDB *db;
                vulkan::Material *mtl;

                const vulkan::VertexShaderModule *vsm=nullptr;

                vulkan::Renderable *render_obj=nullptr;

                int vertex_binding  =-1;
                int color_binding   =-1;
                int normal_binding  =-1;
                int tangent_binding =-1;
                int texcoord_binding=-1;

                VERTEX_VB_FORMAT *vertex    =nullptr;
                VB4f *color     =nullptr;
                VB3f *normal    =nullptr;
                VB3f *tangent   =nullptr;
                VB2f *tex_coord =nullptr;

                vulkan::IndexBuffer *ibo=nullptr;

            public:

                GeometryCreater(SceneDB *sdb,vulkan::Material *m):db(sdb),mtl(m)
                {
                    vsm=mtl->GetVertexShaderModule();

                    vertex_binding  =vsm->GetStageInputBinding("Vertex");
                    color_binding   =vsm->GetStageInputBinding("Color");
                    normal_binding  =vsm->GetStageInputBinding("Normal");
                    tangent_binding =vsm->GetStageInputBinding("Tangent");
                    texcoord_binding=vsm->GetStageInputBinding("TexCoord");
                }

                ~GeometryCreater()
                {
                    if(render_obj)
                        delete render_obj;
                }

                bool Init(uint vertices_number)
                {
                    if(vertex_binding==-1)return(false);
                    if(!vertices_number)return(false);

                    render_obj=mtl->CreateRenderable(vertices_number);

                    vertex      =new VERTEX_VB_FORMAT(vertices_number);
                    if(color_binding    !=-1)color       =new VB4f(vertices_number);
                    if(normal_binding   !=-1)normal      =new VB3f(vertices_number);
                    if(tangent_binding  !=-1)tangent     =new VB3f(vertices_number);
                    if(texcoord_binding !=-1)tex_coord   =new VB2f(vertices_number);

                    return(true);
                }

                void WriteVertex    (const float *v){if(vertex    )vertex     ->BufferData(v);}
                void WriteColor     (const float *v){if(color     )color      ->BufferData(v);}
                void WriteNormal    (const float *v){if(normal    )normal     ->BufferData(v);}
                void WriteTangent   (const float *v){if(tangent   )tangent    ->BufferData(v);}
                void WriteTexCoord  (const float *v){if(tex_coord )tex_coord  ->BufferData(v);}

                VERTEX_VB_FORMAT *  GetVertex     (){if(!vertex)return nullptr;vertex->Begin();return vertex;    }
                VB4f *              GetColor      (){if(!color)return nullptr;color->Begin();return color;     }
                VB3f *              GetNormal     (){if(!normal)return nullptr;normal->Begin();return normal;    }
                VB3f *              GetTangent    (){if(!tangent)return nullptr;tangent->Begin();return tangent;   }
                VB2f *              GetTexCoord   (){if(!tex_coord)return nullptr;tex_coord->Begin();return tex_coord; }

                float *GetVertexPointer     (){return vertex    ?(float *)vertex    ->Begin():nullptr;}
                float *GetColorPointer      (){return color     ?(float *)color     ->Begin():nullptr;}
                float *GetNormalPointer     (){return normal    ?(float *)normal    ->Begin():nullptr;}
                float *GetTangentPointer    (){return tangent   ?(float *)tangent   ->Begin():nullptr;}
                float *GetTexCoordPointer   (){return tex_coord ?(float *)tex_coord ->Begin():nullptr;}

                uint16 *CreateIBO16(uint count,const uint16 *data=nullptr)
                {
                    ibo=db->CreateIBO16(count,data);
                    return (uint16 *)ibo->Map();
                }

                uint32 *CreateIBO32(uint count,const uint32 *data=nullptr)
                {
                    ibo=db->CreateIBO32(count,data);
                    return (uint32 *)ibo->Map();
                }

            private:

                void Finish(int binding,VertexAttribBufferCreater *vb)
                {
                    render_obj->Set(binding,db->CreateVBO(vb));
                    delete vb;
                }

            public:

                vulkan::Renderable *Finish()
                {
                    if(vertex)
                    {
                        render_obj->SetBoundingBox(vertex->GetAABB());
                        Finish(vertex_binding,vertex);
                    }

                    if(color    )Finish(color_binding,      color);
                    if(normal   )Finish(normal_binding,     normal);
                    if(tangent  )Finish(tangent_binding,    tangent);
                    if(tex_coord)Finish(texcoord_binding,   tex_coord);

                    if(ibo)
                    {
                        ibo->Unmap();
                        render_obj->Set(ibo);
                    }

                    vulkan::Renderable *result=render_obj;

                    db->Add(render_obj);
                    render_obj=nullptr;
                    return result;
                }
            };//struct GeometryCreater

            using GeometryCreater3D=GeometryCreater<VB3f>;
            using GeometryCreater2D=GeometryCreater<VB2f>;
        }//namespace

        vulkan::Renderable *CreateRenderableRectangle(SceneDB *db,vulkan::Material *mtl,const RectangleCreateInfo *rci)
        {
            GeometryCreater2D gc(db,mtl);

            if(!gc.Init(4))
                return(nullptr);

            VB2f *vertex=gc.GetVertex();

            vertex->WriteRectFan(rci->scope);

            return gc.Finish();
        }

        vulkan::Renderable *CreateRenderableGBufferComposition(SceneDB *db,vulkan::Material *mtl)
        {
            RectangleCreateInfo rci;

            rci.scope.Set(-1,-1,2,2);

            return CreateRenderableRectangle(db,mtl,&rci);
        }

        vulkan::Renderable *CreateRenderableRoundRectangle(SceneDB *db,vulkan::Material *mtl,const RoundRectangleCreateInfo *rci)
        {
            GeometryCreater2D gc(db,mtl);

            if(rci->radius==0||rci->round_per<=1)      //这是要画矩形
            {
                if(!gc.Init(4))
                    return(nullptr);

                VB2f *vertex=gc.GetVertex();

                vertex->WriteRectFan(rci->scope);
            }
            else
            {
                float radius=rci->radius;

                if(radius>rci->scope.GetWidth()/2.0f)radius=rci->scope.GetWidth()/2.0f;
                if(radius>rci->scope.GetHeight()/2.0f)radius=rci->scope.GetHeight()/2.0f;

                if(!gc.Init(rci->round_per*4))
                    return(nullptr);

                VB2f *vertex=gc.GetVertex();

                vec2<float> *coord=new vec2<float>[rci->round_per];

                float   l=rci->scope.GetLeft(),
                        r=rci->scope.GetRight(),
                        t=rci->scope.GetTop(),
                        b=rci->scope.GetBottom();

                for(uint i=0;i<rci->round_per;i++)
                {
                    float ang=float(i)/float(rci->round_per-1)*90.0f;

                    float x=sin(hgl_ang2rad(ang))*radius;
                    float y=cos(hgl_ang2rad(ang))*radius;

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

            return gc.Finish();
        }

        vulkan::Renderable *CreateRenderableCircle(SceneDB *db,vulkan::Material *mtl,const CircleCreateInfo *cci)
        {
            GeometryCreater2D gc(db,mtl);

            if(!gc.Init(cci->field_count))
                return(nullptr);

            VB2f *vertex=gc.GetVertex();

            for(uint i=0;i<cci->field_count;i++)
            {
                float ang=float(i)/float(cci->field_count)*360.0f;

                float x=cci->center.x+sin(hgl_ang2rad(ang))*cci->radius.x;
                float y=cci->center.y+cos(hgl_ang2rad(ang))*cci->radius.y;

                vertex->Write(x,y);
            }

            return gc.Finish();
        }

        vulkan::Renderable *CreateRenderablePlaneGrid(SceneDB *db,vulkan::Material *mtl,const PlaneGridCreateInfo *pgci)
        {
            GeometryCreater3D gc(db,mtl);

            if(!gc.Init(((pgci->step.u+1)+(pgci->step.v+1))*2))
                return(nullptr);

            VB3f *vertex=gc.GetVertex();
            for(uint row=0;row<=pgci->step.u;row++)
            {
                float pos=float(row)/float(pgci->step.u);

                vertex->WriteLine(  to(pgci->coord[0],pgci->coord[1],pos),
                                    to(pgci->coord[3],pgci->coord[2],pos));
            }

            for(uint col=0;col<=pgci->step.v;col++)
            {
                float pos=float(col)/float(pgci->step.v);

                vertex->WriteLine(to(pgci->coord[1],pgci->coord[2],pos),
                                  to(pgci->coord[0],pgci->coord[3],pos));
            }

            VB4f *color=gc.GetColor();
            if(color)
            {
                for(uint row=0;row<=pgci->step.u;row++)
                {
                    if((row%pgci->side_step.u)==0)
                        color->Fill(pgci->side_color,2);
                    else
                        color->Fill(pgci->color,2);
                }

                for(uint col=0;col<=pgci->step.v;col++)
                {
                    if((col%pgci->side_step.v)==0)
                        color->Fill(pgci->side_color,2);
                    else
                        color->Fill(pgci->color,2);
                }
            }

            return gc.Finish();
        }

        vulkan::Renderable *CreateRenderablePlane(SceneDB *db,vulkan::Material *mtl,const PlaneCreateInfo *pci)
        {
            const   float       xy_vertices [] = { -0.5f,-0.5f,0.0f,  +0.5f,-0.5f,0.0f,    +0.5f,+0.5f,0.0f,    -0.5f,+0.5f,0.0f   };
                    float       xy_tex_coord[] = {  0.0f, 0.0f,        1.0f, 0.0f,          1.0f, 1.0f,          0.0f, 1.0f        };
            const   Vector3f    xy_normal(0.0f,0.0f,1.0f);
            const   Vector3f    xy_tangent(1.0f,0.0f,0.0f);

            GeometryCreater3D gc(db,mtl);

            if(!gc.Init(4))
                return(nullptr);

            gc.WriteVertex(xy_vertices);

            {
                VB3f *normal=gc.GetNormal();

                if(normal)normal->Fill(xy_normal,4);
            }

            {
                VB3f *tangent=gc.GetTangent();

                tangent->Fill(xy_tangent,4);
            }

            {
                VB2f *tex_coord=gc.GetTexCoord();

                if(tex_coord)
                {
                    xy_tex_coord[2]=xy_tex_coord[4]=pci->tile.x;
                    xy_tex_coord[5]=xy_tex_coord[7]=pci->tile.y;

                    tex_coord->BufferData(xy_tex_coord);
                }
            }

            return gc.Finish();
        }

        vulkan::Renderable *CreateRenderableCube(SceneDB *db,vulkan::Material *mtl,const CubeCreateInfo *cci)
        {                              // Points of a cube.
            /*     4            5 */    const float points[]={  -0.5f, -0.5f, -0.5f,    -0.5f, -0.5f, +0.5f,    +0.5f, -0.5f, +0.5f,    +0.5f, -0.5f, -0.5f,    -0.5f, +0.5f, -0.5f,    -0.5f, +0.5f, +0.5f,
            /*     *------------* */                            +0.5f, +0.5f, +0.5f,    +0.5f, +0.5f, -0.5f,    -0.5f, -0.5f, -0.5f,    -0.5f, +0.5f, -0.5f,    +0.5f, +0.5f, -0.5f,    +0.5f, -0.5f, -0.5f,
            /*    /|           /| */                            -0.5f, -0.5f, +0.5f,    -0.5f, +0.5f, +0.5f,    +0.5f, +0.5f, +0.5f,    +0.5f, -0.5f, +0.5f,    -0.5f, -0.5f, -0.5f,    -0.5f, -0.5f, +0.5f,
            /*  0/ |         1/ | */                            -0.5f, +0.5f, +0.5f,    -0.5f, +0.5f, -0.5f,    +0.5f, -0.5f, -0.5f,    +0.5f, -0.5f, +0.5f,    +0.5f, +0.5f, +0.5f,    +0.5f, +0.5f, -0.5f    };
            /*  *--+---------*  | */    // Normals of a cube.
            /*  |  |         |  | */    const float normals[]={ +0.0f, -1.0f, +0.0f,    +0.0f, -1.0f, +0.0f,    +0.0f, -1.0f, +0.0f,    +0.0f, -1.0f, +0.0f,    +0.0f, +1.0f, +0.0f,    +0.0f, +1.0f, +0.0f,
            /*  | 7|         | 6| */                            +0.0f, +1.0f, +0.0f,    +0.0f, +1.0f, +0.0f,    +0.0f, +0.0f, -1.0f,    +0.0f, +0.0f, -1.0f,    +0.0f, +0.0f, -1.0f,    +0.0f, +0.0f, -1.0f,
            /*  |  *---------+--* */                            +0.0f, +0.0f, +1.0f,    +0.0f, +0.0f, +1.0f,    +0.0f, +0.0f, +1.0f,    +0.0f, +0.0f, +1.0f,    -1.0f, +0.0f, +0.0f,    -1.0f, +0.0f, +0.0f,
            /*  | /          | /  */                            -1.0f, +0.0f, +0.0f,    -1.0f, +0.0f, +0.0f,    +1.0f, +0.0f, +0.0f,    +1.0f, +0.0f, +0.0f,    +1.0f, +0.0f, +0.0f,    +1.0f, +0.0f, +0.0f    };
            /*  |/          2|/   */    // The associated indices.
            /* 3*------------*    */    const uint16 indices[]={    0,    2,    1,    0,    3,    2,    4,    5,    6,    4,    6,    7,    8,    9,    10,    8,    10,    11, 12,    15,    14,    12,    14,    13, 16,    17,    18,    16,    18,    19, 20,    23,    22,    20,    22,    21    };

            const float tangents[] = {  +1.0f, 0.0f, 0.0f,      +1.0f, 0.0f, 0.0f,      +1.0f, 0.0f, 0.0f,      +1.0f, 0.0f, 0.0f,      +1.0f, 0.0f, 0.0f,      +1.0f, 0.0f, 0.0f,
                                        +1.0f, 0.0f, 0.0f,      +1.0f, 0.0f, 0.0f,      -1.0f, 0.0f, 0.0f,      -1.0f, 0.0f, 0.0f,      -1.0f, 0.0f, 0.0f,      -1.0f, 0.0f, 0.0f,
                                        +1.0f, 0.0f, 0.0f,      +1.0f, 0.0f, 0.0f,      +1.0f, 0.0f, 0.0f,      +1.0f, 0.0f, 0.0f,       0.0f, 0.0f,+1.0f,       0.0f, 0.0f,+1.0f,
                                         0.0f, 0.0f,+1.0f,       0.0f, 0.0f,+1.0f,       0.0f, 0.0f,-1.0f,       0.0f, 0.0f,-1.0f,       0.0f, 0.0f,-1.0f,       0.0f, 0.0f,-1.0f };

            const float tex_coords[] ={ 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f,
                                        1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f,
                                        0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f };

            GeometryCreater3D gc(db,mtl);

            if(!gc.Init(24))
                return(nullptr);

            if(cci->center  ==Vector3f(0,0,0)
             &&cci->size    ==Vector3f(1,1,1))
            {
                gc.WriteVertex(points);
            }
            else
            {
                const float *sp=points;
                float *vp=gc.GetVertexPointer();

                for(uint i=0;i<24;i++)
                {
                    *vp=cci->center.x+(*sp)*cci->size.x;  ++vp;++sp;
                    *vp=cci->center.y+(*sp)*cci->size.y;  ++vp;++sp;
                    *vp=cci->center.z+(*sp)*cci->size.z;  ++vp;++sp;
                }
            }

            gc.WriteNormal(normals);
            gc.WriteTangent(tangents);

            float *tcp=gc.GetTexCoordPointer();

            if(tcp)
            {
                if(cci->tile.x==1&&cci->tile.y==1)
                {
                    gc.WriteTexCoord(tex_coords);
                }
                else
                {
                    const float *tc=tex_coords;

                    for(uint i=0;i<24;i++)
                    {
                        *tcp=(*tc)*cci->tile.x;++tc;++tcp;
                        *tcp=(*tc)*cci->tile.y;++tc;++tcp;
                    }
                }
            }

            gc.CreateIBO16(6*2*3,indices);
            return gc.Finish();
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
                float halfAngleRadian = hgl_ang2rad(angle) * 0.5f;

                quaternion[0] = 0.0f;
                quaternion[1] = sinf(halfAngleRadian);
                quaternion[2] = 0.0f;
                quaternion[3] = cosf(halfAngleRadian);
            }

            void glusQuaternionRotateRzf(float quaternion[4], const float angle)
            {
                float halfAngleRadian = hgl_ang2rad(angle) * 0.5f;

                quaternion[0] = 0.0f;
                quaternion[1] = 0.0f;
                quaternion[2] = sinf(halfAngleRadian);
                quaternion[3] = cosf(halfAngleRadian);
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
        vulkan::Renderable *CreateRenderableSphere(SceneDB *db,vulkan::Material *mtl,const uint numberSlices)
        {
            GeometryCreater3D gc(db,mtl);

            uint numberParallels = (numberSlices+1) / 2;
            uint numberVertices = (numberParallels + 1) * (numberSlices + 1);
            uint numberIndices = numberParallels * numberSlices * 6;

            const double angleStep = double(2.0f * HGL_PI) / ((double) numberSlices);

            // used later to help us calculating tangents vectors
            float helpVector[3] = { 1.0f, 0.0f, 0.0f };
            float helpQuaternion[4];
            float helpMatrix[16];
            float tex_x;

            if(!gc.Init(numberVertices))
                return(nullptr);

            float *vp=gc.GetVertexPointer();
            float *np=gc.GetNormalPointer();
            float *tp=gc.GetTangentPointer();
            float *tc=gc.GetTexCoordPointer();

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

                    if(tc)
                    {
                        tex_x=(float) j / (float) numberSlices;

                        *tc=tex_x;++tc;
                        *tc=1.0f - (float) i / (float) numberParallels;++tc;

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
                CreateSphereIndices<uint16>(gc.CreateIBO16(numberIndices),numberParallels,numberSlices);
            else
                CreateSphereIndices<uint32>(gc.CreateIBO32(numberIndices),numberParallels,numberSlices);

            return gc.Finish();
        }

        vulkan::Renderable *CreateRenderableDome(SceneDB *db,vulkan::Material *mtl,const DomeCreateInfo *dci)
        {
            GeometryCreater3D gc(db,mtl);

            uint i, j;

            uint numberParallels = dci->numberSlices / 4;
            uint numberVertices = (numberParallels + 1) * (dci->numberSlices + 1);
            uint numberIndices = numberParallels * dci->numberSlices * 6;

            float angleStep = (2.0f * HGL_PI) / ((float) dci->numberSlices);

            // used later to help us calculating tangents vectors
            float helpVector[3] = { 1.0f, 0.0f, 0.0f };
            float helpQuaternion[4];
            float helpMatrix[16];
            float tex_x;

            if (dci->numberSlices < 3 || numberVertices > GLUS_MAX_VERTICES || numberIndices > GLUS_MAX_INDICES)
                return nullptr;

            if(!gc.Init(numberVertices))
                return(nullptr);

            float *vp=gc.GetVertexPointer();
            float *np=gc.GetNormalPointer();
            float *tp=gc.GetTangentPointer();
            float *tc=gc.GetTexCoordPointer();

            for (i = 0; i < numberParallels + 1; i++)
            {
                for (j = 0; j < dci->numberSlices + 1; j++)
                {
                    uint vertexIndex = (i * (dci->numberSlices + 1) + j) * 4;
                    uint normalIndex = (i * (dci->numberSlices + 1) + j) * 3;
                    uint tangentIndex = (i * (dci->numberSlices + 1) + j) * 3;
                    uint texCoordsIndex = (i * (dci->numberSlices + 1) + j) * 2;

                    float x= dci->radius * sinf(angleStep * (float) i) * sinf(angleStep * (float) j);
                    float y= dci->radius * sinf(angleStep * (float) i) * cosf(angleStep * (float) j);
                    float z= dci->radius * cosf(angleStep * (float) i);

                    *vp=x;++vp;
                    *vp=y;++vp;
                    *vp=z;++vp;

                    if(np)
                    {
                        *np = x / dci->radius;++np;
                        *np = y / dci->radius;++np;
                        *np = z / dci->radius;++np;
                    }

                    if(tc)
                    {
                        *tc = tex_x=(float) j / (float) dci->numberSlices;++tc;
                        *tc = 1.0f - (float) i / (float) numberParallels;++tc;

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
                CreateSphereIndices<uint16>(gc.CreateIBO16(numberIndices),numberParallels,dci->numberSlices);
            else
                CreateSphereIndices<uint32>(gc.CreateIBO32(numberIndices),numberParallels,dci->numberSlices);

            return gc.Finish();
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

        vulkan::Renderable *CreateRenderableTorus(SceneDB *db,vulkan::Material *mtl,const TorusCreateInfo *tci)
        {
            GeometryCreater3D gc(db,mtl);

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

            if(!gc.Init(numberVertices))
                return(nullptr);

            float *vp=gc.GetVertexPointer();
            float *np=gc.GetNormalPointer();
            float *tp=gc.GetTangentPointer();
            float *tc=gc.GetTexCoordPointer();

            // generate vertices and its attributes
            for (sideCount = 0; sideCount <= tci->numberSlices; ++sideCount, s += sIncr)
            {
                // precompute some values
                cos2PIs = (float) cosf(2.0f * HGL_PI * s);
                sin2PIs = (float) sinf(2.0f * HGL_PI * s);

                t = 0.0f;
                for (faceCount = 0; faceCount <= tci->numberStacks; ++faceCount, t += tIncr)
                {
                    // precompute some values
                    cos2PIt = (float) cosf(2.0f * HGL_PI * t);
                    sin2PIt = (float) sinf(2.0f * HGL_PI * t);

                    // generate vertex and stores it in the right position
                    *vp = (centerRadius + torusRadius * cos2PIt) * cos2PIs; ++vp;
                    *vp = torusRadius * sin2PIt; ++vp;
                    *vp = (centerRadius + torusRadius * cos2PIt) * sin2PIs; ++vp;

                    if(np)
                    {
                        // generate normal and stores it in the right position
                        // NOTE: cos (2PIx) = cos (x) and sin (2PIx) = sin (x) so, we can use this formula
                        //       normal = {cos(2PIs)cos(2PIt) , sin(2PIs)cos(2PIt) ,sin(2PIt)}
                        *np = cos2PIs * cos2PIt; ++np;
                        *np = sin2PIt; ++np;
                        *np = sin2PIs * cos2PIt; ++np;
                    }

                    if(tc)
                    {
                        // generate texture coordinates and stores it in the right position
                        *tc = s; ++tc;
                        *tc = t; ++tc;
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
                CreateTorusIndices<uint16>(gc.CreateIBO16(numberIndices),tci->numberSlices,tci->numberStacks);
            else
                CreateTorusIndices<uint32>(gc.CreateIBO32(numberIndices),tci->numberSlices,tci->numberStacks);

            return gc.Finish();
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

        vulkan::Renderable *CreateRenderableCylinder(SceneDB *db,vulkan::Material *mtl,const CylinderCreateInfo *cci)
        {
            uint numberIndices = cci->numberSlices * 3 * 2 + cci->numberSlices * 6;

            if(numberIndices<=0)
                return(nullptr);

            GeometryCreater3D gc(db,mtl);

            uint numberVertices = (cci->numberSlices + 2) * 2 + (cci->numberSlices + 1) * 2;

            if(!gc.Init(numberVertices))
                return(nullptr);

            float angleStep = (2.0f * HGL_PI) / ((float) cci->numberSlices);

            if (cci->numberSlices < 3 || numberVertices > GLUS_MAX_VERTICES || numberIndices > GLUS_MAX_INDICES)
                return nullptr;

            float *vp=gc.GetVertexPointer();
            float *np=gc.GetNormalPointer();
            float *tp=gc.GetTangentPointer();
            float *tc=gc.GetTexCoordPointer();

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

            if(tc)
            {
                *tc = 0.0f; ++tc;
                *tc = 0.0f; ++tc;
            }

            for(uint i = 0; i < cci->numberSlices + 1; i++)
            {
    	        float currentAngle = angleStep * (float)i;

		        *vp =  cosf(currentAngle) * cci->radius;++vp;
		        *vp = -sinf(currentAngle) * cci->radius;++vp;
		        *vp = -cci->halfExtend;                 ++vp;

                if(np)
                {
                    *np = 0.0f; ++np;
                    *np = 0.0f; ++np;
                    *np =-1.0f; ++np;
                }

                if(tp)
                {
                    *tp = sinf(currentAngle);   ++tp;
                    *tp = cosf(currentAngle);   ++tp;
                    *tp = 0.0f;                 ++tp;
                }

                if(tc)
                {
                    *tc = 0.0f; ++tc;
                    *tc = 0.0f; ++tc;
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

            if(tc)
            {
                *tc = 1.0f; ++tc;
                *tc = 1.0f; ++tc;
            }

            for(uint i = 0; i < cci->numberSlices + 1; i++)
            {
    	        float currentAngle = angleStep * (float)i;

		        *vp =  cosf(currentAngle) * cci->radius;++vp;
		        *vp = -sinf(currentAngle) * cci->radius;++vp;
		        *vp =  cci->halfExtend;                 ++vp;

                if(np)
                {
                    *np = 0.0f;    ++np;
                    *np = 0.0f;    ++np;
                    *np = 1.0f;    ++np;
                }

                if(tp)
                {
                    *tp = -sinf(currentAngle);  ++tp;
                    *tp = -cosf(currentAngle);  ++tp;
                    *tp = 0.0f;                 ++tp;
                }

                if(tc)
                {
                    *tc = 1.0f; ++tc;
                    *tc = 1.0f; ++tc;
                }
            }

            for(uint i = 0; i < cci->numberSlices + 1; i++)
            {
		        float currentAngle = angleStep * (float)i;

		        float sign = -1.0f;

		        for (uint j = 0; j < 2; j++)
                {
			        *vp =  cosf(currentAngle) * cci->radius;    ++vp;
			        *vp = -sinf(currentAngle) * cci->radius;    ++vp;
			        *vp =  cci->halfExtend * sign;              ++vp;

                    if(np)
                    {
			            *np = cosf(currentAngle);   ++np;
			            *np = -sinf(currentAngle);  ++np;
			            *np = 0.0f;                 ++np;
                    }

                    if(tp)
                    {
			            *tp = -sinf(currentAngle);  ++tp;
			            *tp = -cosf(currentAngle);  ++tp;
			            *tp = 0.0f;                 ++tp;
                    }

                    if(tc)
                    {
			            *tc = (float)i / (float)cci->numberSlices;  ++tc;
			            *tc = (sign + 1.0f) / 2.0f;                 ++tc;
                    }

			        sign = 1.0f;
                }
            }

            if(numberVertices<=0xffff)
                CreateCylinderIndices<uint16>(gc.CreateIBO16(numberIndices),cci->numberSlices);
            else
                CreateCylinderIndices<uint32>(gc.CreateIBO32(numberIndices),cci->numberSlices);

            return gc.Finish();
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

        vulkan::Renderable *CreateRenderableCone(SceneDB *db,vulkan::Material *mtl,const ConeCreateInfo *cci)
        {
            GeometryCreater3D gc(db,mtl);

            uint i, j;

            uint numberVertices = (cci->numberSlices + 2) + (cci->numberSlices + 1) * (cci->numberStacks + 1);

            if(!gc.Init(numberVertices))
                return(nullptr);

            uint numberIndices = cci->numberSlices * 3 + cci->numberSlices * 6 * cci->numberStacks;

            float angleStep = (2.0f * HGL_PI) / ((float) cci->numberSlices);

            float h = 2.0f * cci->halfExtend;
            float r = cci->radius;
            float l = sqrtf(h*h + r*r);

            if (cci->numberSlices < 3 || cci->numberStacks < 1 || numberVertices > GLUS_MAX_VERTICES || numberIndices > GLUS_MAX_INDICES)
                return nullptr;

            float *vp=gc.GetVertexPointer();
            float *np=gc.GetNormalPointer();
            float *tp=gc.GetTangentPointer();
            float *tc=gc.GetTexCoordPointer();

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

            if(tc)
            {
                *tc = 0.0f; ++tc;
                *tc = 0.0f; ++tc;
            }

            for (i = 0; i < cci->numberSlices + 1; i++)
            {
    	        float currentAngle = angleStep * (float)i;

		        *vp =  cosf(currentAngle) * cci->radius;++vp;
		        *vp = -sinf(currentAngle) * cci->radius;++vp;
		        *vp = -cci->halfExtend;                 ++vp;

                if(np)
                {
		            *np = 0.0f;++np;
		            *np = 0.0f;++np;
		            *np =-1.0f;++np;
                }

                if(tp)
                {
		            *tp = sinf(currentAngle);   ++tp;
		            *tp = cosf(currentAngle);   ++tp;
		            *tp = 0.0f;                 ++tp;
                }

                if(tc)
                {
		            *tc = 0.0f; ++tc;
		            *tc = 0.0f; ++tc;
                }
            }

	        for (j = 0; j < cci->numberStacks + 1; j++)
            {
		        float level = (float)j / (float)cci->numberStacks;

		        for (i = 0; i < cci->numberSlices + 1; i++)
		        {
			        float currentAngle = angleStep * (float)i;

			        *vp =  cosf(currentAngle) * cci->radius * (1.0f - level);   ++vp;
			        *vp = -sinf(currentAngle) * cci->radius * (1.0f - level);   ++vp;
			        *vp = -cci->halfExtend + 2.0f * cci->halfExtend * level;    ++vp;

                    if(np)
                    {
			            *np = h / l *  cosf(currentAngle);  ++np;
			            *np = h / l * -sinf(currentAngle);  ++np;
			            *np = r / l;                        ++np;
                    }

                    if(tp)
                    {
			            *tp = -sinf(currentAngle);  ++tp;
			            *tp = -cosf(currentAngle);  ++tp;
			            *tp = 0.0f;                 ++tp;
                    }

                    if(tc)
                    {
			            *tc = (float)i / (float)cci->numberSlices;  ++tc;
			            *tc = level;                                ++tc;
                    }
                }
            }

            if(numberVertices<=0xffff)
                CreateConeIndices<uint16>(gc.CreateIBO16(numberIndices),cci->numberSlices,cci->numberStacks);
            else
                CreateConeIndices<uint32>(gc.CreateIBO32(numberIndices),cci->numberSlices,cci->numberStacks);

            return gc.Finish();
        }

        vulkan::Renderable *CreateRenderableAxis(SceneDB *db,vulkan::Material *mtl,const AxisCreateInfo *aci)
        {
            GeometryCreater3D gc(db,mtl);

            if(!gc.Init(6))
                return(nullptr);

            VB3f *vertex=gc.GetVertex();
            VB4f *color=gc.GetColor();

            if(!vertex||!color)
                return(nullptr);

            vertex->Write(aci->root);color->Write(aci->color[0]);
            vertex->Write(aci->root.x+aci->size[0],aci->root.y,aci->root.z);color->Write(aci->color[0]);

            vertex->Write(aci->root);color->Write(aci->color[1]);
            vertex->Write(aci->root.x,aci->root.y+aci->size[1],aci->root.z);color->Write(aci->color[1]);

            vertex->Write(aci->root);color->Write(aci->color[2]);
            vertex->Write(aci->root.x,aci->root.y,aci->root.z+aci->size[2]);color->Write(aci->color[2]);

            return gc.Finish();
        }

        vulkan::Renderable *CreateRenderableBoundingBox(SceneDB *db,vulkan::Material *mtl,const CubeCreateInfo *cci)
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

            GeometryCreater3D gc(db,mtl);

            if(!gc.Init(8))
                return(nullptr);

            VB3f *vertex=gc.GetVertex();

            if(!vertex)return(nullptr);

            if(cci->center  ==Vector3f(0,0,0)
             &&cci->size    ==Vector3f(1,1,1))
            {
                gc.WriteVertex(points);
            }
            else
            {
                const float *sp=points;
                float *vp=(float *)(vertex->Get());

                for(uint i=0;i<8;i++)
                {
                    *vp=cci->center.x+(*sp)*cci->size.x;  ++vp;++sp;
                    *vp=cci->center.y+(*sp)*cci->size.y;  ++vp;++sp;
                    *vp=cci->center.z+(*sp)*cci->size.z;  ++vp;++sp;
                }
            }

            if(cci->has_color)
            {
                float *color_pointer=gc.GetColorPointer();

                if(color_pointer)
                {
                    for(uint i=0;i<8;i++)
                    {
                        memcpy(color_pointer,&(cci->color),4*sizeof(float));
                        color_pointer+=4;
                    }
                }
            }

            gc.CreateIBO16(24,indices);

            return gc.Finish();
        }
    }//namespace graph
}//namespace hgl
