// Equirectangular ( or spherical)
// 等距矩形（或球形）贴图反射测试

#include"VulkanAppFramework.h"
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/Mesh.h>
#include<hgl/graph/VKTexture.h>
#include<hgl/graph/RenderList.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH=1280;
constexpr uint32_t SCREEN_HEIGHT=720;

class TestApp:public CameraAppFramework
{
private:

    SceneNode           render_root;
    RenderList *        render_list         =nullptr;

    MaterialInstance *  envmap_mi           =nullptr;

    Pipeline *          solid_pipeline      =nullptr;

    DeviceBuffer *         ubo_light           =nullptr;
    DeviceBuffer *         ubo_phong           =nullptr;

    Sampler *           sampler             =nullptr;
    Texture2D *         texture             =nullptr;

    Primitive *        ro_sphere           =nullptr;

private:

    bool InitMaterial()
    {
        {
            // photo source: https://www.hqreslib.com
            // license: CY-BY

            texture=db->LoadTexture2D(OS_TEXT("res/equirectangular/jifu.Tex2D"),false);

            if(!texture)
                return(false);

            sampler=db->CreateSampler();
        }

        {
            envmap_mi=db->CreateMaterialInstance(OS_TEXT("res/material/EnvEquirectangularMap"));
            if(!envmap_mi)return(false);

            if(!envmap_mi->BindImageSampler(DescriptorSetType::Value,"Envmap"    ,texture,    sampler))return(false);
        }

        solid_pipeline=CreatePipeline(envmap_mi,InlinePipeline::Solid3D,Prim::Triangles);

        return(true);
    }

    void CreateRenderObject()
    {
        ro_sphere=inline_geometry::CreateSphere(db,envmap_mi->GetVIL(),128);
    }

    bool InitUBO()
    {
        if(!BindCameraUBO(envmap_mi))return(false);

        return(true);
    }

    SceneNode *Add(Primitive *r,MaterialInstance *mi,Pipeline *pl)
    {
        auto ri=db->CreateRenderable(r,mi,pl);

        return render_root.CreateSubNode(ri);
    }

    bool InitScene()
    {
        auto sn_sphere=Add(ro_sphere,envmap_mi,solid_pipeline);
        sn_sphere->SetLocalMatrix(scale(5,5,5));

        render_root.RefreshMatrix();
        render_list->Expend(GetCameraInfo(),&render_root);

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

        camera->pos=Vector3f(10,10,0);

        camera_control->SetTarget(Vector3f(0,0,0));

        render_list=new RenderList(device);

        if(!InitMaterial())
            return(false);

        if(!InitUBO())
            return(false);

        CreateRenderObject();

        if(!InitScene())
            return(false);

        return(true);
    }

    void BuildCommandBuffer(uint32_t index) override
    {
        render_root.RefreshMatrix();
        render_list->Expend(GetCameraInfo(),&render_root);

        VulkanApplicationFramework::BuildCommandBuffer(index,render_list);
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
