// SimplestAxis
// 直接从0,0,0向三个方向画一条直线，用于确认坐标轴方向

#include<hgl/WorkManager.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/RenderCollector.h>
#include<hgl/graph/camera/Camera.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/graph/VKVertexInputConfig.h>
#include<hgl/graph/camera/FirstPersonCameraControl.h>
#include<hgl/color/Color.h>
#include<hgl/component/MeshComponent.h>

using namespace hgl;
using namespace hgl::graph;

class TestApp:public WorkObject
{
private:

    Material *          material            =nullptr;
    Pipeline *          pipeline            =nullptr;

    Primitive *         prim_axis           =nullptr;
    MaterialInstance *  material_instance   =nullptr;

private:

    bool InitMDP()
    {
        mtl::Material3DCreateConfig cfg(PrimitiveType::Lines);

        cfg.local_to_world=true;

        material_instance=CreateMaterialInstance(mtl::inline_material::VertexColor3D,&cfg);

        pipeline=CreatePipeline(material_instance,InlinePipeline::Solid3D);

        return pipeline;
    }

    bool CreateRenderObject()
    {
        using namespace inline_geometry;

        auto pc=GetPrimitiveCreater(material_instance);

        inline_geometry::AxisCreateInfo aci;

        prim_axis=CreateAxis(pc,&aci);

        return prim_axis;
    }

    bool InitScene()
    {
        Mesh *ri=CreateMesh(prim_axis,material_instance,pipeline);

        CreateComponentInfo cci(GetSceneRoot());

        CreateComponent<MeshComponent>(&cci,ri);

        CameraControl *camera_control=GetCameraControl();

        camera_control->SetPosition(Vector3f(32,32,32));
        camera_control->SetTarget(Vector3f(0,0,0));

//        camera_control->SetReserveDirection(true,true);        //反转x,y

        return(true);
    }

public:

    using WorkObject::WorkObject;

    ~TestApp()
    {
        SAFE_CLEAR(prim_axis);
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
    return RunFramework<TestApp>(OS_TEXT("SimplestAxis"),1280,720);
}
