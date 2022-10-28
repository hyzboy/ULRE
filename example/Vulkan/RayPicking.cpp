// 18.RayPicking

#include"VulkanAppFramework.h"
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/RenderList.h>
#include<hgl/graph/Camera.h>
#include<hgl/graph/Ray.h>
#include<hgl/graph/VKVertexAttribBuffer.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH=1280;
constexpr uint32_t SCREEN_HEIGHT=720;

static float position_data[2][3]=
{
    {100,100,100},
    {0,0,0}
};

static float color_data[2][4]=
{
    {1,1,0,1},
    {1,1,0,1}
};

class TestApp:public CameraAppFramework
{
    Color4f color;

    DeviceBuffer *ubo_color=nullptr;

private:

    SceneNode           render_root;
    RenderList *        render_list         =nullptr;

    Material *          material            =nullptr;
    MaterialInstance *  material_instance   =nullptr;
    Pipeline *          pipeline            =nullptr;

    Primitive *        ro_plane_grid       =nullptr;

    Primitive *        ro_line             =nullptr;

    VBO *               vbo_pos             =nullptr;

    Ray                 ray;

private:

    bool InitMDP()
    {
        material=db->CreateMaterial(OS_TEXT("res/material/VertexColor3D"));
        if(!material)return(false);

        material_instance=db->CreateMaterialInstance(material);
        if(!material_instance)return(false);
        
        pipeline=CreatePipeline(material_instance,InlinePipeline::Solid3D,Prim::Lines);
        if(!pipeline)
            return(false);

        return(true);
    }
    
    Renderable *Add(Primitive *r,const Matrix4f &mat)
    {
        Renderable *ri=db->CreateRenderable(r,material_instance,pipeline);

        render_root.CreateSubNode(mat,ri);

        return ri;
    }

    bool CreateRenderObject()
    {
        using namespace inline_geometry;

        {
            struct PlaneGridCreateInfo pgci;

            pgci.coord[0]=Vector3f(-100,-100,0);
            pgci.coord[1]=Vector3f( 100,-100,0);
            pgci.coord[2]=Vector3f( 100, 100,0);
            pgci.coord[3]=Vector3f(-100, 100,0);

            pgci.step.x=32;
            pgci.step.y=32;

            pgci.side_step.x=8;
            pgci.side_step.y=8;

            pgci.color.Set(0.25,0,0,1);
            pgci.side_color.Set(1,0,0,1);

            const VIL *vil=material_instance->GetVIL();

            ro_plane_grid=CreatePlaneGrid(db,vil,&pgci);
        }

        {
            ro_line=db->CreatePrimitive(2);
            if(!ro_line)return(false);
            
            if(!ro_line->Set(VAN::Position,  vbo_pos=db->CreateVBO(VF_V3F,2,position_data   )))return(false);
            if(!ro_line->Set(VAN::Color,         db->CreateVBO(VF_V4F,2,color_data      )))return(false);
        }

        return(true);
    }

    bool InitScene()
    {
        Add(ro_plane_grid,Matrix4f(1.0f));
        Add(ro_line,Matrix4f(1.0f));

        camera->pos=Vector3f(100,100,50);
        camera_control->SetTarget(Vector3f(0,0,0));
        camera_control->Refresh();

        render_root.RefreshMatrix();
        render_list->Expend(camera->info,&render_root);

        return(true);
    }

public:

    ~TestApp()
    {
        SAFE_CLEAR(render_list);
    }

    bool Init()
    {
        if(!CameraAppFramework::Init(SCREEN_WIDTH,SCREEN_HEIGHT))
            return(false);
            
        render_list=new RenderList(device);

        if(!InitMDP())
            return(false);

        if(!CreateRenderObject())
            return(false);

        if(!InitScene())
            return(false);

        return(true);
    }

    void BuildCommandBuffer(uint32 index)
    {
        const CameraInfo &ci=GetCameraInfo();

        ray.Set(GetMouseCoord(),&ci);                           //设置射线查询的屏幕坐标点

        const Vector3f pos=ray.ClosestPoint(Vector3f(0,0,0));   //求射线上与点(0,0,0)最近的点的坐标

        vbo_pos->Write(&pos,3*sizeof(float));                   //更新VBO上这个点的位置

        render_root.RefreshMatrix();
        render_list->Expend(ci,&render_root);

        VulkanApplicationFramework::BuildCommandBuffer(index,render_list);
    }

    void Resize(int w,int h)override
    {
        CameraAppFramework::Resize(w,h);
        
        VulkanApplicationFramework::BuildCommandBuffer(render_list);
    }
};//class TestApp:public CameraAppFramework

int main(int,char **)
{
    TestApp app;

    if(!app.Init())
        return(-1);

    while(app.Run());

    return 0;
}
