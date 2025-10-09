#include<hgl/WorkManager.h>
#include<hgl/graph/VertexDataManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/GeometryCreater.h>
#include<hgl/graph/RenderCollector.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/color/Color.h>
#include<hgl/component/PrimitiveComponent.h>

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
    COLOR::CherryBlossomPink,

    COLOR::SkyBlue,
    COLOR::GrassGreen,
    COLOR::BloodRed,
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
        Geometry *geometry;
        Primitive *primitive;
        PrimitiveComponentData *data;
        ComponentDataPtr cdp;

        PrimitiveComponent *component;

    public:

        ~RenderMesh()
        {
            cdp.unref();
            delete primitive;
            delete geometry;
        }
    };

    RenderMesh *rm_floor=nullptr;           //地板
    RenderMesh *rm_box=nullptr;             //边框包围盒

    ArrayList<RenderMesh *> render_mesh;

private:

    bool InitMaterialInstance(MaterialData *md)
    {
        if(!md)
            return(false);

        Color4f color;

        for(size_t i=0;i<COLOR_COUNT;i++)
        {
            color=GetColor4f(TestColor[i],1.0);

            md->mi[i]=CreateMaterialInstance(md->material,(VIL *)nullptr,&color);

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

    RenderMesh *CreateRenderMesh(Geometry *geometry,MaterialData *md,const int color)
    {
        if(!geometry)
            return(nullptr);

        Primitive *primitive=CreatePrimitive(geometry,md->mi[color],md->pipeline);

        if(!primitive)
            return nullptr;

        RenderMesh *rm=new RenderMesh;

        rm->geometry=geometry;
        rm->primitive=primitive;
        rm->data=new PrimitiveComponentData(primitive);
        rm->cdp =rm->data;

        return rm;
    }

    bool CreateGeometryMesh()
    {
        using namespace inline_geometry;

        GeometryCreater *prim_creater=new GeometryCreater(mesh_vdm);

        if(!prim_creater)
            return(false);

        //
        rm_floor=CreateRenderMesh(CreatePlaneSqaure(prim_creater),&solid,0);

        //Sphere
        render_mesh.Add(CreateRenderMesh(CreateSphere(prim_creater,64),&solid,1));

        //Cone
        {
            struct ConeCreateInfo cci;

            cci.radius      =1;         //圆锥半径
            cci.halfExtend  =1;         //圆锤一半高度
            cci.numberSlices=64;        //圆锥底部分割数
            cci.numberStacks=4;         //圆锥高度分割数

            render_mesh.Add(CreateRenderMesh(CreateCone(prim_creater,&cci),&solid,2));
        }

        //Cyliner
        {
            struct CylinderCreateInfo cci;

            cci.halfExtend  =1.25;      //圆柱一半高度
            cci.numberSlices=16;        //圆柱底部分割数
            cci.radius      =1.25f;     //圆柱半径

            render_mesh.Add(CreateRenderMesh(CreateCylinder(prim_creater,&cci),&solid,3));
        }

        //Torus
        {
            struct TorusCreateInfo tci;

            tci.innerRadius=1.9;
            tci.outerRadius=2.1;
            tci.numberSlices=128;
            tci.numberStacks=16;

            render_mesh.Add(CreateRenderMesh(CreateTorus(prim_creater,&tci),&solid,4));
        }

        {
            struct HollowCylinderCreateInfo hcci;

            hcci.halfExtend    =1.25;      //圆柱一半高度
            hcci.innerRadius   =0.8f;      //内半径
            hcci.outerRadius   =1.25f;     //外半径
            hcci.numberSlices  =64;        //圆柱底部分割数

            render_mesh.Add(CreateRenderMesh(CreateHollowCylinder(prim_creater,&hcci),&solid,5));
        }

        {
            struct HexSphereCreateInfo hsci;

            hsci.subdivisions=3;

            render_mesh.Add(CreateRenderMesh(CreateHexSphere(prim_creater,&hsci),&solid,6));
        }

        {
            struct CapsuleCreateInfo cci;

            render_mesh.Add(CreateRenderMesh(CreateCapsule(prim_creater,&cci),&solid,7));
        }

        {
            TaperedCapsuleCreateInfo tcci;

            tcci.topRadius=0.1;

            render_mesh.Add(CreateRenderMesh(CreateTaperedCapsule(prim_creater,&tcci),&solid,8));
        }

        delete prim_creater;
        return(true);
    }

    bool CreateBoundingBoxMesh()
    {
        using namespace inline_geometry;

        auto pc=GetGeometryCreater(wire.material);

        inline_geometry::BoundingBoxCreateInfo bbci;

        rm_box=CreateRenderMesh(CreateBoundingBox(pc,&bbci),&wire,5);

        return rm_box;
    }

    bool InitScene()
    {
        CreateComponentInfo cci(GetWorldRootNode());

        {
            cci.mat=AxisZRotate(45.0f);

            rm_floor->component=CreateComponent<PrimitiveComponent>(&cci,rm_floor->cdp);
        }

        int i=0;
        const float rm_count=render_mesh.GetCount();

        for(auto rm:render_mesh)
        {
            if(!rm)continue;
            //螺旋排列
            cci.mat=AxisZRotate(deg2rad(360.0f*i/rm_count))*TranslateMatrix(3,0,0);

            rm->component=CreateComponent<PrimitiveComponent>(&cci,rm->cdp);
            rm->component->SetOverrideMaterial(solid.mi[i%COLOR_COUNT]);

            ++i;
        }

        return(true);
    }

    bool InitBoundingBoxScene()
    {
        SceneNode *root=GetWorldRootNode();

        CreateComponentInfo cci(root);

        ArrayList<Matrix4f> box_matrices;

        for(Component *c:root->GetComponents())
        {
            if(c->GetTypeHash()!=PrimitiveComponent::StaticTypeHash())
                continue;

            PrimitiveComponent *component=(PrimitiveComponent *)c;

            Matrix4f mat;

            if(component->GetWorldOBBMatrix(mat))
                box_matrices.Add(mat);
        }

        //不可以直接在上面的循环中创建新的Component，因为循环本身就要读取Component列表

        for(const Matrix4f &mat:box_matrices)
        {
            cci.mat=mat;

            if(!CreateComponent<PrimitiveComponent>(&cci,rm_box->cdp))
                return(false);
        }

        return(true);
    }

    void SetCamera()
    {
        CameraControl *camera_control=GetCameraControl();

        camera_control->SetPosition(Vector3f(8,8,8));
        camera_control->SetTarget(Vector3f(0,0,0));
    }

public:

    using WorkObject::WorkObject;

    ~TestApp()
    {
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

        SetCamera();

        return(true);
    }
};//class TestApp:public CameraAppFramework

int os_main(int,os_char **)
{
    return RunFramework<TestApp>(OS_TEXT("Render Bounding Box"),1280,720);
}
