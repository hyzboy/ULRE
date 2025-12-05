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

VK_NAMESPACE_BEGIN
Geometry *LoadGeometry(VulkanDevice *device,const VIL *vil,const OSString &filename);
VK_NAMESPACE_END

constexpr const COLOR TestColor[] =
{
    COLOR::MozillaCharcoal,
    COLOR::MozillaSand,

    COLOR::BlenderAxisRed,
    COLOR::BlenderAxisGreen,
    COLOR::BlenderAxisBlue,

    COLOR::BananaYellow,
    COLOR::CherryBlossomPink,

    COLOR::SkyBlue,
    //COLOR::Amber
};

constexpr const size_t COLOR_COUNT = sizeof(TestColor) / sizeof(COLOR);

class TestApp:public WorkObject
{
private:

    struct MaterialData
    {
        Material *material = nullptr;
        const VIL *vil = nullptr;

        Pipeline *pipeline = nullptr;
        MaterialInstance *mi[COLOR_COUNT]{};
    };

    MaterialData solid;
    MaterialData wire;

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

    RenderMesh *rm_box=nullptr;             //边框包围盒

    ArrayList<RenderMesh *> render_mesh;

private:

    bool InitMaterialInstance(MaterialData *md)
    {
        if(!md)
            return(false);

        Color4f color;

        for(size_t i = 0;i < COLOR_COUNT;i++)
        {
            color = GetColor4f(TestColor[i],1.0);

            md->mi[i] = CreateMaterialInstance(md->material,(VIL *)nullptr,&color);

            if(!md->mi[i])
                return(false);
        }

        md->vil = md->material->GetDefaultVIL();

        if(!md->vil)
            return(false);

        md->pipeline = CreatePipeline(md->material,InlinePipeline::Solid3D);

        return md->pipeline;
    }

    bool InitSolidMDP()
    {
        mtl::Material3DCreateConfig cfg(PrimitiveType::Triangles);

        mtl::MaterialCreateInfo *mci = mtl::CreateGizmo3D(GetDevAttr(),&cfg);

        if(!mci)
            return(false);

        solid.material = CreateMaterial("Gizmo3D",mci);

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

    bool CreateBoundingBoxMesh()
    {
        using namespace inline_geometry;

        auto pc=GetGeometryCreater(wire.material);

        inline_geometry::BoundingBoxCreateInfo bbci;

        rm_box=CreateRenderMesh(CreateBoundingBox(pc,&bbci),&wire,5);

        return rm_box;
    }

    RenderMesh *CreateRenderMesh(Geometry *geometry,MaterialData *md,const int color)
    {
        if(!geometry)
            return(nullptr);

        Primitive *primitive = CreatePrimitive(geometry,md->mi[color],md->pipeline);

        if(!primitive)
            return nullptr;

        RenderMesh *rm = new RenderMesh;

        rm->geometry = geometry;
        rm->primitive = primitive;
        rm->data = new PrimitiveComponentData(primitive);
        rm->cdp = rm->data;

        return rm;
    }

    bool CreateGeometryMesh()
    {
        int count=0;

        for(int i=0;i< COLOR_COUNT;i++)
        {
            OSString fn = OSString(OS_TEXT("res/model/Chess/ABeautifulGame.")) + OSString::numberOf(i) + OS_TEXT(".geometry");

            Geometry *geo = LoadGeometry(GetDevice(),solid.vil,fn);

            if(!geo)
                continue;

            RenderMesh *rm=CreateRenderMesh(geo,&solid,i);

            if(!rm)
            {
                delete geo;
                continue;
            }

            {
                CreateComponentInfo cci(GetWorldRootNode());

                //螺旋排列
                cci.mat = AxisZRotate(deg2rad(360.0f * i / COLOR_COUNT)) * TranslateMatrix(0.25,0,0);

                rm->component = CreateComponent<PrimitiveComponent>(&cci,rm->cdp);
                rm->component->SetOverrideMaterial(solid.mi[i]);

                ++count;
            }

            render_mesh.Add(rm);
        }

        return(count>0);
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
        CameraControl *camera_control = GetCameraControl();

        RenderMesh *rm=*render_mesh.At(0);

        math::AABB aabb;
        rm->component->GetWorldAABB(aabb);

        camera_control->SetPosition(aabb.GetMax()*Vector3f(2,2,2));
        camera_control->SetTarget(Vector3f(0,0,0));
    }

public:

    using WorkObject::WorkObject;

    bool Init() override
    {
        if(!InitSolidMDP())
            return(false);

        if(!InitWireMDP())
            return(false);

        if(!CreateGeometryMesh())
            return(false);

        if(!CreateBoundingBoxMesh())
            return(false);

        if(!InitBoundingBoxScene())
            return(false);

        SetCamera();

        return(true);
    }
};//class TestApp:public CameraAppFramework

int os_main(int,os_char **)
{
    return RunFramework<TestApp>(OS_TEXT("Render Geometry"),1280,720);
}
