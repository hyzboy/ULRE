// TestCameraGeometry
// Test the camera geometry generator

#include<hgl/WorkManager.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/RenderList.h>
#include<hgl/graph/Camera.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/graph/VKVertexInputConfig.h>
#include<hgl/graph/FirstPersonCameraControl.h>
#include<hgl/color/Color.h>
#include<hgl/component/MeshComponent.h>

using namespace hgl;
using namespace hgl::graph;

class TestApp:public WorkObject
{
private:

    Material *          material            =nullptr;
    Pipeline *          pipeline            =nullptr;

    Primitive *         prim_camera         =nullptr;
    MaterialInstance *  material_instance   =nullptr;

private:

    bool InitMDP()
    {
        mtl::Material3DCreateConfig cfg(PrimitiveType::Triangles);

        cfg.local_to_world=true;
        cfg.vertex_color=true;

        material_instance=CreateMaterialInstance(mtl::inline_material::VertexColor3D,&cfg);

        pipeline=CreatePipeline(material_instance,InlinePipeline::Solid3D);

        return pipeline;
    }

    bool CreateRenderObject()
    {
        using namespace inline_geometry;

        auto pc=GetPrimitiveCreater(material_instance);

        inline_geometry::CameraCreateInfo cci;
        cci.body_size = 2.0f;
        cci.lens_size = 0.4f;
        cci.lens_depth = 0.6f;
        cci.body_color = {0.3f, 0.3f, 0.3f, 1.0f};  // 深灰色主体
        cci.lens_color = {0.1f, 0.1f, 0.1f, 1.0f};  // 黑色镜头

        prim_camera=CreateCamera(pc,&cci);

        return prim_camera;
    }

    bool InitScene()
    {
        Mesh *ri=CreateMesh(prim_camera,material_instance,pipeline);

        CreateComponentInfo cci(GetSceneRoot());

        CreateComponent<MeshComponent>(&cci,ri);

        CameraControl *camera_control=GetCameraControl();

        camera_control->SetPosition(Vector3f(5,5,5));
        camera_control->SetTarget(Vector3f(0,0,0));

        return(true);
    }

public:

    using WorkObject::WorkObject;

    ~TestApp()
    {
        SAFE_CLEAR(prim_camera);
    }

    bool Init() override
    {
        if(!InitMDP())
            return(false);

        if(!CreateRenderObject())
            return(false);

        if(!InitScene())
            return(false);

        return(true);
    }
};//class TestApp:public CameraAppFramework

int os_main(int,os_char **)
{
    WorkManager wm(U"TestCameraGeometry");

    wm.SetActiveNode(new TestApp(U"TestCameraGeometry"));

    return wm.Run();
}