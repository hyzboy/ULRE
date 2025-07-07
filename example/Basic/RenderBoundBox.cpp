#include<hgl/WorkManager.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/VertexDataManager.h>
#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/PrimitiveCreater.h>
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

constexpr const COLOR TestColor[]=
{
    COLOR::MozillaCharcoal,
    COLOR::MozillaSand,

    COLOR::BlenderAxisRed,
    COLOR::BlenderAxisGreen,
    COLOR::BlenderAxisBlue,

    COLOR::BananaYellow,
};

constexpr const size_t COLOR_COUNT=sizeof(TestColor)/sizeof(COLOR);

class TestApp:public WorkObject
{
private:

    struct MaterialData
    {
        Material *          material          = nullptr;
        const VIL *         vil               = nullptr;

        Pipeline *          pipeline          = nullptr;
        MaterialInstance *  mi[COLOR_COUNT]{};
    };

    MaterialData solid;
    MaterialData wire;

    VertexDataManager * mesh_vdm=nullptr;

    struct RenderMesh
    {
        Primitive *prim;
        Mesh *mesh;
        MeshComponentData *data;
        ComponentDataPtr cdp;

        MeshComponent *component;

    public:

        ~RenderMesh()
        {
            cdp.unref();
            delete mesh;
            delete prim;
        }
    };

    RenderMesh  *rm_plane=nullptr,
                *rm_sphere=nullptr,
                *rm_cone=nullptr,
                *rm_cylinder=nullptr,
                *rm_torus=nullptr,
                *rm_box=nullptr;

private:

    bool InitMaterialInstance(MaterialData *md)
    {
        if(!md)
            return(false);

        Color4f color;

        for(size_t i=0;i<COLOR_COUNT;i++)
        {
            color=GetColor4f(TestColor[i],1.0);

            md->mi[i]=CreateMaterialInstance(md->material,nullptr,&color);

            if(!md->mi[i])
                return(false);
        }

        md->vil=md->material->GetDefaultVIL();

        if(!md->vil)
            return(false);

        md->pipeline=CreatePipeline(md->material,InlinePipeline::Solid3D);

        return md->pipeline;
    }

    bool InitSolidMDP()
    {
        mtl::Material3DCreateConfig cfg(PrimitiveType::Triangles);

        mtl::MaterialCreateInfo *mci=mtl::CreateGizmo3D(GetDevAttr(),&cfg);

        if(!mci)
            return(false);

        solid.material=CreateMaterial("Gizmo3D",mci);

        return InitMaterialInstance(&solid);
    }

    bool InitWireMDP()
    {
        mtl::Material3DCreateConfig cfg(PrimitiveType::Lines);

        mtl::MaterialCreateInfo *mci=mtl::CreatePureColor3D(GetDevAttr(),&cfg);

        if(!mci)
            return(false);

        wire.material=CreateMaterial("PureColorLine3D",mci);

        return InitMaterialInstance(&wire);
    }

    bool InitVDM()
    {
        mesh_vdm=CreateVDM(solid.vil,HGL_SIZE_1MB);

        if(!mesh_vdm)
            return(false);

        return true;
    }

    RenderMesh *CreateRenderMesh(Primitive *prim,MaterialData *md,const int color)
    {
        if(!prim)
            return(nullptr);

        Mesh *mesh=graph::CreateMesh(prim,md->mi[color],md->pipeline);

        if(!mesh)
            return nullptr;

        RenderMesh *rm=new RenderMesh;

        rm->prim=prim;
        rm->mesh=mesh;
        rm->data=new MeshComponentData(mesh);
        rm->cdp =rm->data;

        return rm;
    }

    bool CreateGeometryMesh()
    {
        using namespace inline_geometry;

        PrimitiveCreater *prim_creater=new PrimitiveCreater(mesh_vdm);

        if(!prim_creater)
            return(false);

        rm_plane=CreateRenderMesh(CreatePlaneSqaure(prim_creater),&solid,0);

        //Sphere
        rm_sphere=CreateRenderMesh(CreateSphere(prim_creater,16),&solid,1);

        //Cone
        {
            struct ConeCreateInfo cci;

            cci.radius      =1;         //圆锥半径
            cci.halfExtend  =1;         //圆锤一半高度
            cci.numberSlices=16;        //圆锥底部分割数
            cci.numberStacks=8;         //圆锥高度分割数

            rm_cone=CreateRenderMesh(CreateCone(prim_creater,&cci),&solid,2);
        }

        //Cyliner
        {
            struct CylinderCreateInfo cci;

            cci.halfExtend  =1.25;      //圆柱一半高度
            cci.numberSlices=16;        //圆柱底部分割数
            cci.radius      =1.25f;     //圆柱半径

            rm_cylinder=CreateRenderMesh(CreateCylinder(prim_creater,&cci),&solid,3);
        }

        //Torus
        {
            struct TorusCreateInfo tci;

            tci.innerRadius=0.9;
            tci.outerRadius=1.1;
            tci.numberSlices=64;
            tci.numberStacks=8;

            rm_torus=CreateRenderMesh(CreateTorus(prim_creater,&tci),&solid,4);
        }

        delete prim_creater;
        return(true);
    }

    bool CreateBoundingBoxMesh()
    {
        using namespace inline_geometry;

        auto pc=GetPrimitiveCreater(wire.material);

        inline_geometry::BoundingBoxCreateInfo bbci;

        rm_box=CreateRenderMesh(CreateBoundingBox(pc,&bbci),&wire,5);

        return rm_box;
    }

    bool InitScene()
    {
        CreateComponentInfo cci(GetSceneRoot());

        {
            cci.mat=ScaleMatrix(10,10,1);

            rm_plane->component=CreateComponent<MeshComponent>(&cci,rm_plane->cdp);
        }

        {
            cci.mat=AxisYRotate(deg2rad(90));

            rm_torus->component=CreateComponent<MeshComponent>(&cci,rm_torus->cdp);
            rm_torus->component->SetOverrideMaterial(solid.mi[1]);
        }

        return(true);
    }

    bool InitBoundingBoxScene()
    {
        CreateComponentInfo cci(GetSceneRoot());

        //cci.mat=rm_torus->component->GetLocalMatrix()*rm_torus->component->GetMesh()->GetBoundingBox();

        CreateComponent<MeshComponent>(&cci,rm_box->cdp);

        CameraControl *camera_control=GetCameraControl();

        camera_control->SetPosition(Vector3f(8,8,8));
        camera_control->SetTarget(Vector3f(0,0,0));

        return(true);
    }

public:

    using WorkObject::WorkObject;

    ~TestApp()
    {
        SAFE_CLEAR(rm_box)

        SAFE_CLEAR(rm_torus)
        SAFE_CLEAR(rm_cylinder)
        SAFE_CLEAR(rm_cone)
        SAFE_CLEAR(rm_sphere)
        SAFE_CLEAR(rm_plane)

        SAFE_CLEAR(mesh_vdm)
    }

    bool Init() override
    {
        if(!InitSolidMDP())
            return(false);

        if(!InitWireMDP())
            return(false);

        if(!InitVDM())
            return(false);

        if(!CreateGeometryMesh())
            return(false);

        if(!CreateBoundingBoxMesh())
            return(false);

        if(!InitScene())
            return(false);

        if(!InitBoundingBoxScene())
            return(false);

        return(true);
    }
};//class TestApp:public CameraAppFramework

int os_main(int,os_char **)
{
    return RunFramework<TestApp>(OS_TEXT("Render Bounding Box"),1280,720);
}
