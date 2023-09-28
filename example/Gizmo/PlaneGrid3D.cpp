// 4.Geometry3D

#include"VulkanAppFramework.h"
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/RenderList.h>
#include<hgl/graph/Camera.h>
#include<hgl/graph/VKRenderablePrimitiveCreater.h>
#include<hgl/graph/mtl/3d/Material3DCreateConfig.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH=1280;
constexpr uint32_t SCREEN_HEIGHT=720;

constexpr const Vector4f GridColor[3]=
{
    {1,0,0,1},
    {0,1,0,1},
    {0,0,1,1}
};

class TestApp:public CameraAppFramework
{
    Color4f color;

    DeviceBuffer *ubo_color=nullptr;

private:

    SceneNode           render_root;
    RenderList *        render_list         =nullptr;

    Material *          material            =nullptr;
    Pipeline *          pipeline            =nullptr;

    Primitive *         ro_plane_grid       =nullptr;
    MaterialInstance *  material_instance[3]{};

private:

    bool InitMDP()
    {
        mtl::Material3DCreateConfig cfg(device->GetDeviceAttribute(),"VertexLuminance3D",Prim::Lines);

        cfg.local_to_world=true;

        AutoDelete<mtl::MaterialCreateInfo> mci=mtl::CreateVertexLuminance3D(&cfg);

        material=db->CreateMaterial(mci);
        if(!material)return(false);

        db->global_descriptor.Bind(material);

        for(uint i=0;i<3;i++)
        {
            material_instance[i]=db->CreateMaterialInstance(material);

            material_instance[i]->WriteMIData(GridColor[i]);
        }
        
        pipeline=CreatePipeline(material,InlinePipeline::Solid3D,Prim::Lines);

        return pipeline;
    }

    bool CreateRenderObject()
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

        pgci.lum=0.5;
        pgci.side_lum=1.0;

        ro_plane_grid=CreatePlaneGrid(db,material->GetDefaultVIL(),&pgci);

        return ro_plane_grid;
    }
    
    Renderable *Add(MaterialInstance *mi,const Matrix4f &mat)
    {
        Renderable *ri=db->CreateRenderable(ro_plane_grid,mi,pipeline);

        if(!ri)
            return(nullptr);

        render_root.CreateSubNode(mat,ri);

        return ri;
    }

    bool InitScene()
    {
        Add(material_instance[0],Matrix4f(1.0f));
        Add(material_instance[1],rotate(HGL_RAD_90,0,1,0));
        Add(material_instance[2],rotate(HGL_RAD_90,1,0,0));

        camera->pos=Vector3f(200,200,200);
        camera_control->SetTarget(Vector3f(0,0,0));
        camera_control->Refresh();

        render_root.RefreshMatrix();
        render_list->Expend(&render_root);

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
