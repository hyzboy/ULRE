#include<hgl/WorkManager.h>
#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/component/MeshComponent.h>
#include<hgl/graph/mtl/UBOCommon.h>

using namespace hgl;
using namespace hgl::graph;

class TestApp:public WorkObject
{
private:

    struct RenderMesh
    {
        MaterialInstance *  mi  =nullptr;
        Pipeline *          pl  =nullptr;
        Primitive *         prim=nullptr;
        Mesh *              mesh=nullptr;
        MeshComponentData * data=nullptr;
        ComponentDataPtr    cdp;

        MeshComponent *component;

    public:

        ~RenderMesh()
        {
            cdp.unref();
            SAFE_CLEAR(mesh);
            SAFE_CLEAR(prim);
        }
    };

    RenderMesh cube,sphere;

private:

    bool InitSphere()
    {
        Color4f SphereColor(0.5,0.7,1,1);

        sphere.mi=CreateMaterialInstance(mtl::inline_material::Gizmo3D,(VIL *)nullptr,&SphereColor);

        if(!sphere.mi)
            return(false);

        sphere.pl=CreatePipeline(sphere.mi);

        if(!sphere.pl)
            return(false);

        PrimitiveCreater *pc=GetPrimitiveCreater(sphere.mi);

        if(!pc)
            return(false);

        inline_geometry::HexSphereCreateInfo hsci;

        hsci.subdivisions=3;
        hsci.radius=1.0f;

        sphere.prim=inline_geometry::CreateHexSphere(pc,&hsci);

        if(!sphere.prim)
            return(false);

        sphere.mesh=CreateMesh(sphere.prim,sphere.mi,sphere.pl);

        if(!sphere.mesh)
            return(false);


    }

    bool InitCubeMDP()
    {
        cube.mi=CreateMaterialInstance(mtl::inline_material::BasicLit);

        if(!cube.mi)
            return(false);

        cube.pl=CreatePipeline(cube.mi);

        return cube.pl;
    }

    bool CreateRenderObject()
    {
        using namespace inline_geometry;

        auto pc=GetPrimitiveCreater(cube.mi);

        struct CubeCreateInfo cci;

        cube.prim=CreateCube(pc,&cci);

        return prim_sky_sphere;
    }

    bool InitScene()
    {
        {
            Mesh *ri=CreateMesh(prim_sky_sphere,mi_sky_sphere,mtl_pipeline);

            CreateComponentInfo cci(GetSceneRoot());

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
