#include<hgl/WorkManager.h>
#include<hgl/graph/VertexDataManager.h>
#include<hgl/graph/geo/Wall.h>
#include<hgl/graph/PrimitiveCreater.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/color/Color.h>
#include<hgl/component/MeshComponent.h>
#include<hgl/graph/VKTexture.h>

using namespace hgl;
using namespace hgl::graph;

class TestApp:public WorkObject
{
private:
    Material *material = nullptr;
    MaterialInstance *material_instance = nullptr;
    Pipeline *pipeline = nullptr;

    VertexDataManager *mesh_vdm = nullptr;

    Mesh *wall_mesh = nullptr;
    MeshComponent *wall_component = nullptr;

public:
    using WorkObject::WorkObject;

    ~TestApp()
    {
        SAFE_CLEAR(mesh_vdm)
    }

    bool Init() override
    {
        mtl::Material3DCreateConfig cfg(PrimitiveType::Triangles);
        mtl::MaterialCreateInfo *mci = mtl::CreateGizmo3D(GetDevAttr(), &cfg);
        if(!mci) return false;

        material = CreateMaterial("Gizmo3D_Walls", mci);
        if(!material) return false;

        Color4f color = GetColor4f(COLOR::BananaYellow, 1.0f);
        material_instance = CreateMaterialInstance(material, (VIL *)nullptr, &color);
        if(!material_instance) return false;

        pipeline = CreatePipeline(material, InlinePipeline::Solid3D);
        if(!pipeline) return false;

        const VIL *vil = material->GetDefaultVIL();
        mesh_vdm = CreateVDM(vil, HGL_SIZE_1MB);
        if(!mesh_vdm) return false;

        PrimitiveCreater *pc = new PrimitiveCreater(mesh_vdm);

        using namespace inline_geometry;

        std::vector<Vector2f> verts;
        verts.push_back(Vector2f(-3.0f, 0.0f)); //0
        verts.push_back(Vector2f(-1.5f, 1.0f)); //1
        verts.push_back(Vector2f(0.0f, 0.0f));  //2
        verts.push_back(Vector2f(1.5f, 1.2f));  //3
        verts.push_back(Vector2f(3.0f, 0.0f));  //4
        verts.push_back(Vector2f(4.0f, -0.8f)); //5

        uint idx[] = {0,1, 1,2, 2,3, 3,4, 4,5};

        WallCreateInfo wci;
        wci.vertices = verts.data();
        wci.vertexCount = (uint)verts.size();
        wci.indices = idx;
        wci.indexCount = sizeof(idx)/sizeof(idx[0]);
        wci.thickness = 0.2f;
        wci.height = 2.0f;
        wci.cornerJoin=WallCreateInfo::CornerJoin::Round;

        // UV settings
        wci.uv_tile_v = 2.0f; // tile twice along height
        wci.uv_u_repeat_per_unit = 0.5f; // repeat 0.5 times per unit length (i.e. long walls tile less)

        // load texture (user will replace path later)
        Texture2D *tex = LoadTexture2D(OS_TEXT("res/image/Brickwall/Albedo.Tex2D"), true);
        Sampler *samp = CreateSampler();

        // bind texture to material
        if(!material->BindImageSampler(DescriptorSetType::PerMaterial, mtl::SamplerName::BaseColor, tex, samp))
        {
            // continue even if binding failed in this example
        }

        Primitive *prim = CreateWallsFromLines2D(pc, &wci);

        delete pc;

        if(!prim) return false;

        wall_mesh = CreateMesh(prim, material_instance, pipeline);
        if(!wall_mesh) return false;

        CreateComponentInfo cci(GetSceneRoot());
        MeshComponentData *mcd = new MeshComponentData(wall_mesh);
        ComponentDataPtr cdp = mcd;

        wall_component = CreateComponent<MeshComponent>(&cci, cdp);
        if(!wall_component) return false;

        CameraControl *cc = GetCameraControl();
        cc->SetPosition(Vector3f(6,6,6));
        cc->SetTarget(Vector3f(0,0,0));

        return true;
    }
};

int os_main(int,os_char **)
{
    return RunFramework<TestApp>(OS_TEXT("Walls From Polyline Texture Example"), 1280, 720);
}
