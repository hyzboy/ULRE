// SimplestAxis
// 直接从0,0,0向三个方向画一条直线，用于确认坐标轴方向

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
        Date d;
        Time t;

        d.Sync();
        t.Sync();

        ubo_sky_info=GetDevice()->CreateUBO<UBOSkyInfo>(&mtl::SBS_SkyInfo);

        ubo_sky_info->data()->SetLocation(geo::CityID::BeiJing);
        ubo_sky_info->data()->SetDate(d);
        ubo_sky_info->data()->SetByTimeOfDay(6,0,0);

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

        prim_sky_sphere=CreateSphere(pc,16);

        return prim_sky_sphere;
    }

    bool InitScene()
    {
        {
            Mesh *ri=CreateMesh(prim_sky_sphere,mi_sky_sphere,mtl_pipeline);

            CreateComponentInfo cci(GetSceneRoot());

            cci.mat=ScaleMatrix(100);

            CreateComponent<MeshComponent>(&cci,ri);
        }

        CameraControl *camera_control=GetCameraControl();

        camera_control->SetPosition(Vector3f(32,32,32));
        camera_control->SetTarget(Vector3f(0,0,0));

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
    return RunFramework<TestApp>(OS_TEXT("SimplestAxis"),1280,720);
}
