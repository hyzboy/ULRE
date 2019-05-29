#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/VertexBuffer.h>
#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/graph/vulkan/VKShaderModule.h>
#include<hgl/graph/SceneDB.h>
namespace hgl
{
    namespace graph
    {
        vulkan::Renderable *CreateRectangle(SceneDB *db,vulkan::Material *mtl,const RectangleCreateInfo *rci)
        {
            const vulkan::VertexShaderModule *vsm=mtl->GetVertexShaderModule();

            const int vertex_binding=vsm->GetStageInputBinding("Vertex");

            if(vertex_binding==-1)
                return(nullptr);
            
            VB2f *vertex=new VB2f(4);

            vertex->Begin();
            vertex->WriteRectFan(rci->scope);
            vertex->End();

            vulkan::Renderable *render_obj=mtl->CreateRenderable(vertex->GetCount());
            render_obj->Set(vertex_binding,db->CreateVBO(vertex));
            render_obj->SetBoundingBox(vertex->GetAABB());

            delete vertex;
            db->Add(render_obj);
            return render_obj;
        }

        vulkan::Renderable *CreateRoundRectangle(SceneDB *db,vulkan::Material *mtl,const RoundRectangleCreateInfo *rci)
        {
            const vulkan::VertexShaderModule *vsm=mtl->GetVertexShaderModule();

            vulkan::Renderable *render_obj=nullptr;
            const int vertex_binding=vsm->GetStageInputBinding("Vertex");

            if(vertex_binding==-1)
                return(nullptr);

            VB2f *vertex=nullptr;

            if(rci->radius==0||rci->round_per<=1)      //这是要画矩形
            {
                vertex=new VB2f(4);

                vertex->Begin();
                vertex->WriteRectFan(rci->scope);
                vertex->End();
            }
            else
            {
                float radius=rci->radius;

                if(radius>rci->scope.GetWidth()/2.0f)radius=rci->scope.GetWidth()/2.0f;
                if(radius>rci->scope.GetHeight()/2.0f)radius=rci->scope.GetHeight()/2.0f;

                vertex=new VB2f(rci->round_per*4);

                vertex->Begin();

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

                vertex->End();
            }

            render_obj=mtl->CreateRenderable(vertex->GetCount());
            render_obj->Set(vertex_binding,db->CreateVBO(vertex));
            render_obj->SetBoundingBox(vertex->GetAABB());

            delete vertex;
            db->Add(render_obj);
            return render_obj;
        }

        vulkan::Renderable *CreateCircle(SceneDB *db,vulkan::Material *mtl,const CircleCreateInfo *cci)
        {            
            const vulkan::VertexShaderModule *vsm=mtl->GetVertexShaderModule();

            const int vertex_binding=vsm->GetStageInputBinding("Vertex");

            if(vertex_binding==-1)
                return(nullptr);

            VB2f *vertex=new VB2f(cci->field_count+2);

            vertex->Begin();

            vertex->Write(cci->center.x,cci->center.y);

            for(uint i=0;i<=cci->field_count;i++)
            {
                float ang=float(i)/float(cci->field_count)*360.0f;

                float x=cci->center.x+sin(hgl_ang2rad(ang))*cci->radius.x;
                float y=cci->center.y+cos(hgl_ang2rad(ang))*cci->radius.y;
                    
                vertex->Write(x,y);
            }

            vertex->End();

            vulkan::Renderable *render_obj=mtl->CreateRenderable(vertex->GetCount());
            render_obj->Set(vertex_binding,db->CreateVBO(vertex));
            render_obj->SetBoundingBox(vertex->GetAABB());

            delete vertex;
            db->Add(render_obj);
            return render_obj;
        }

        vulkan::Renderable *CreatePlaneGrid(SceneDB *db,vulkan::Material *mtl,const PlaneGridCreateInfo *pgci)
        {
            const vulkan::VertexShaderModule *vsm=mtl->GetVertexShaderModule();

            const int vertex_binding=vsm->GetStageInputBinding("Vertex");

            if(vertex_binding==-1)
                return(nullptr);

            VB3f *vertex=new VB3f(((pgci->step.u+1)+(pgci->step.v+1))*2);

            vertex->Begin();
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
            vertex->End();

            vulkan::Renderable *render_obj=mtl->CreateRenderable(vertex->GetCount());
            render_obj->Set(vertex_binding,db->CreateVBO(vertex));
            render_obj->SetBoundingBox(vertex->GetAABB());

            const int color_binding=vsm->GetStageInputBinding("Color");
            if(color_binding!=-1)
            {
                VB4f *color=new VB4f(((pgci->step.u+1)+(pgci->step.v+1))*2);

                color->Begin();
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
                color->End();

                render_obj->Set(color_binding,db->CreateVBO(color));
            }

            delete vertex;
            db->Add(render_obj);
            return render_obj;
        }

        vulkan::Renderable *CreatePlane(SceneDB *db,vulkan::Material *mtl,const PlaneCreateInfo *pci)
        {
            const   float       xy_vertices [] = { -0.5f,-0.5f,0.0f,  +0.5f,-0.5f,0.0f,    -0.5f,+0.5f,0.0f,   +0.5f,+0.5f,0.0f};
                    float       xy_tex_coord[] = {  0.0f, 0.0f,        1.0f,0.0f,           0.0f,1.0f,          1.0f, 1.0f};
            const   Vector3f    xy_normal(0.0f,0.0f,1.0f);
            const   Vector3f    xy_tangent(1.0f,0.0f,0.0f);

            const vulkan::VertexShaderModule *vsm=mtl->GetVertexShaderModule();

            vulkan::Renderable *render_obj=nullptr;
            {
                const int vertex_binding=vsm->GetStageInputBinding("Vertex");

                if(vertex_binding==-1)
                    return(nullptr);

                VB3f *vertex=new VB3f(4,xy_vertices);

                render_obj=mtl->CreateRenderable(vertex->GetCount());
                render_obj->Set(vertex_binding,db->CreateVBO(vertex));
                render_obj->SetBoundingBox(vertex->GetAABB());
                delete vertex;
            }

            if(render_obj)
            {
                const int normal_binding=vsm->GetStageInputBinding("Normal");

                if(normal_binding!=-1)
                {
                    VB3f *normal=new VB3f(4);

                    normal->Begin();
                    normal->Fill(xy_normal,4);
                    normal->End();

                    render_obj->Set(normal_binding,db->CreateVBO(normal));
                    delete normal;
                }

                const int tagent_binding=vsm->GetStageInputBinding("Tangent");

                if(tagent_binding!=-1)
                {
                    VB3f *tangent=new VB3f(4);

                    tangent->Begin();
                    tangent->Fill(xy_tangent,4);
                    tangent->End();

                    render_obj->Set(tagent_binding,db->CreateVBO(tangent));
                    delete tangent;
                }

                const int texcoord_binding=vsm->GetStageInputBinding("TexCoord");

                if(texcoord_binding!=-1)
                {
                    xy_tex_coord[2]=xy_tex_coord[6]=pci->tile.x;
                    xy_tex_coord[5]=xy_tex_coord[7]=pci->tile.y;
                    
                    VB2f *tex_coord=new VB2f(4,xy_tex_coord);

                    render_obj->Set(texcoord_binding,db->CreateVBO(tex_coord));
                    delete tex_coord;
                }
            }

            db->Add(render_obj);
            return render_obj;
        }

        vulkan::Renderable *CreateCube(SceneDB *db,vulkan::Material *mtl,const CubeCreateInfo *cci)
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

                  float tex_coords[] ={ 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f,
                                        1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f,
                                        0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f };

            const vulkan::VertexShaderModule *vsm=mtl->GetVertexShaderModule();

            vulkan::Renderable *render_obj=nullptr;
            {
                const int vertex_binding=vsm->GetStageInputBinding("Vertex");

                if(vertex_binding==-1)
                    return(nullptr);

                VB3f *vertex=new VB3f(24,points);

                render_obj=mtl->CreateRenderable(vertex->GetCount());
                render_obj->Set(vertex_binding,db->CreateVBO(vertex));
                render_obj->SetBoundingBox(vertex->GetAABB());
                delete vertex;
            }

            const int normal_binding=vsm->GetStageInputBinding("Normal");

            if(normal_binding!=-1)
            {
                VB3f *normal=new VB3f(24,normals);

                render_obj->Set(normal_binding,db->CreateVBO(normal));
                delete normal;
            }

            const int tagent_binding=vsm->GetStageInputBinding("Tangent");

            if(tagent_binding!=-1)
            {
                VB3f *tangent=new VB3f(24,tangents);

                render_obj->Set(tagent_binding,db->CreateVBO(tangent));
                delete tangent;
            }

            const int texcoord_binding=vsm->GetStageInputBinding("TexCoord");

            if(texcoord_binding!=-1)
            {
                float *tc=tex_coords;
                for(uint i=0;i<24;i++)
                {
                    (*tc)*=cci->tile.x;++tc;
                    (*tc)*=cci->tile.y;++tc;
                }

                VB2f *tex_coord=new VB2f(24,tex_coords);

                render_obj->Set(texcoord_binding,db->CreateVBO(tex_coord));
                delete tex_coord;
            }

            render_obj->Set(db->CreateIBO16(6*2*3,indices));
            return(render_obj);
        }

        vulkan::Renderable *CreateBoundingBox(SceneDB *db,vulkan::Material *mtl,const BoundingBoxCreateInfo *bbci)
        {
            vec side[24];

            bbci->bounding_box.ToEdgeList(side);

            const vulkan::VertexShaderModule *vsm=mtl->GetVertexShaderModule();

            vulkan::Renderable *render_obj=nullptr;
            {
                const int vertex_binding=vsm->GetStageInputBinding("Vertex");

                if(vertex_binding==-1)
                    return(nullptr);

                VB3f *vertex=new VB3f(24);

                vertex->Begin();
                    vertex->Write(side,24);
                vertex->End();

                render_obj=mtl->CreateRenderable(vertex->GetCount());
                render_obj->Set(vertex_binding,db->CreateVBO(vertex));
                render_obj->SetBoundingBox(vertex->GetAABB());
                delete vertex;
            }

            return render_obj;
        }

        //vulkan::Renderable *CreateSphere(SceneDB *db,vulkan::Material *mtl,const SphereCreateInfo *sci)
        //{
        //}
    }//namespace graph
}//namespace hgl