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
    }//namespace graph
}//namespace hgl