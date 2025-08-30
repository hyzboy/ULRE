#include<hgl/WorkManager.h>
#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/component/MeshComponent.h>
#include<hgl/graph/Sky.h>
#include<hgl/graph/mtl/UBOCommon.h>

using namespace hgl;
using namespace hgl::graph;

class TestApp:public WorkObject
{
private:

    using UBOSkyInfo=UBOInstance<SkyInfo>;

    UBOSkyInfo *        ubo_sky_info        =nullptr;

    Material *          mtl_sky_sphere      =nullptr;
    Pipeline *          mtl_pipeline        =nullptr;

    Primitive *         prim_sky_sphere     =nullptr;
    MaterialInstance *  mi_sky_sphere       =nullptr;

private:

    void InitSky()
    {
        ubo_sky_info=GetDevice()->CreateUBO<UBOSkyInfo>(&mtl::SBS_SkyInfo);

        auto *sky_info=ubo_sky_info->data();

        sky_info->SetTime(12,30,0);           // 正午 12:30

        auto *desc_bind=GetScene()->GetDescriptorBinding();

        desc_bind->AddUBO(ubo_sky_info);
    }

    bool InitMDP()
    {
        mtl::SkyMinimalCreateConfig cfg;

        mi_sky_sphere=CreateMaterialInstance(mtl::inline_material::SkyMinimal,&cfg);

        mtl_pipeline=CreatePipeline(mi_sky_sphere,InlinePipeline::Sky);

        return mtl_pipeline;
    }

    bool CreateRenderObject()
    {
        using namespace inline_geometry;

        auto pc=GetPrimitiveCreater(mi_sky_sphere);

        prim_sky_sphere=CreateSphere(pc,64);

        return prim_sky_sphere;
    }

    bool InitScene()
    {
        {
            Mesh *ri=CreateMesh(prim_sky_sphere,mi_sky_sphere,mtl_pipeline);

            CreateComponentInfo cci(GetSceneRoot());

            cci.mat=ScaleMatrix(160);

            CreateComponent<MeshComponent>(&cci,ri);
        }

        CameraControl *camera_control=GetCameraControl();

        camera_control->SetPosition(Vector3f(0,0,0));
        camera_control->SetTarget(Vector3f(10,10,10));

        //        camera_control->SetReserveDirection(true,true);        //反转x,y

        return(true);
    }

public:

    using WorkObject::WorkObject;

    bool Init() override
    {
        InitSky();

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
    return RunFramework<TestApp>(OS_TEXT("SimplestAtmosphere"),1280,720);
}
