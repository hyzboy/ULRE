// 最简单的地形渲染
// 在CPU端生成网格，在vertex中取样高度图

#include"VulkanAppFramework.h"
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/RenderList.h>
#include<hgl/graph/Camera.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH=1280;
constexpr uint32_t SCREEN_HEIGHT=720;

        struct TerrainCreateInfo
        {
            uint Xcount;            ///<X轴网格数
            uint Ycount;            ///<Y轴网格数

            struct //翻倍边缘
            {
                bool top,left,right,bottom;
            }dual;
        };//struct TerrainCreateInfo

        /**
         * 创建一个地形网格
         */
        Primitive *CreateRenderableTerrain(RenderResource *db,const VIL *vil,const TerrainCreateInfo *tci)
        {
        }

class TestApp:public CameraAppFramework
{
    Color4f color;

    GPUBuffer *ubo_color=nullptr;

private:

    SceneNode   render_root;
    RenderList *render_list=nullptr;

    Material *          material            =nullptr;
    MaterialInstance *  material_instance   =nullptr;
    Pipeline *          pipeline            =nullptr;

    Primitive        * renderable          =nullptr;

private:

    bool InitMDP()
    {
        material=db->CreateMaterial(OS_TEXT("res/material/VertexColor3D"));
        if(!material)return(false);

        material_instance=db->CreateMaterialInstance(material);
        if(!material_instance)return(false);
        
        pipeline=CreatePipeline(material_instance,InlinePipeline::Solid3D,Prim::TriangleStrip);
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

    void CreateRenderObject()
    {
        using namespace inline_geometry;

        struct PlaneGridCreateInfo pgci;

        pgci.coord[0]=Vector3f(-100,-100,0);
        pgci.coord[1]=Vector3f( 100,-100,0);
        pgci.coord[2]=Vector3f( 100, 100,0);
        pgci.coord[3]=Vector3f(-100, 100,0);

        pgci.step.x=32;
        pgci.step.y=32;

        pgci.side_step.x=8;
        pgci.side_step.y=8;

        pgci.color.Set(0.75,0.75,0.75);
        pgci.side_color.Set(1,0,0,1);

        const VIL *vil=material_instance->GetVIL();

        renderable=CreatePlaneGrid(db,vil,&pgci);
    }

    bool InitScene()
    {
        Renderable *ri=db->CreateRenderable(renderable,material_instance,pipeline);

        render_root.CreateSubNode(ri);

        camera->pos=Vector3f(200,200,200);
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

        CreateRenderObject();

        if(!InitScene())
            return(false);

        return(true);
    }

    void BuildCommandBuffer(uint32 index)
    {
        render_root.RefreshMatrix();
        render_list->Expend(GetCameraInfo(),&render_root);

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
