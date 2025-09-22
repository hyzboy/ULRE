#include<hgl/WorkManager.h>
#include<hgl/graph/VertexDataManager.h>
#include<hgl/graph/geo/Wall.h>
#include<hgl/graph/PrimitiveCreater.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/color/Color.h>
#include<hgl/component/MeshComponent.h>

using namespace hgl;
using namespace hgl::graph;

class TestApp:public WorkObject
{
private:

    mtl::BasicLitMaterialInstance mi_data;

    Material *material = nullptr;
    MaterialInstance *material_instance = nullptr;
    Pipeline *pipeline = nullptr;

    VertexDataManager *mesh_vdm = nullptr;

    std::vector<SubMesh*> wall_meshes;
    std::vector<MeshComponent*> wall_components;

public:
    using WorkObject::WorkObject;

    ~TestApp()
    {
        SAFE_CLEAR(mesh_vdm)
    }

    bool Init() override
    {
        mtl::BasicLitMaterialCreateConfig cfg;
        mtl::MaterialCreateInfo *mci = mtl::CreateBasicLit(GetDevAttr(), &cfg);

        if(!mci) return false;

        mi_data.base_color = GetRGBA(COLOR::FireBrick);
        mi_data.metallic=0;
        mi_data.roughness=0.95f;
        mi_data.fresnel=0.04f;
        mi_data.ibl_intensity=0.0f;

        material = CreateMaterial("Gizmo3D_Walls", mci);
        if(!material) return false;

        material_instance = CreateMaterialInstance(material, (VIL *)nullptr, &mi_data);
        if(!material_instance) return false;

        pipeline = CreatePipeline(material, InlinePipeline::Solid3D);
        if(!pipeline) return false;

        const VIL *vil = material->GetDefaultVIL();
        mesh_vdm = CreateVDM(vil, HGL_SIZE_1MB);
        if(!mesh_vdm) return false;

        PrimitiveCreater *pc = new PrimitiveCreater(mesh_vdm);

        using namespace inline_geometry;

        // Build several different polylines to create a more complex wall scene

        // 1) long zig-zag
        {
            std::vector<Vector2f> verts;
            verts.push_back(Vector2f(-6.0f, -1.0f));
            verts.push_back(Vector2f(-4.5f,  1.2f));
            verts.push_back(Vector2f(-3.0f, -0.4f));
            verts.push_back(Vector2f(-1.5f,  1.6f));
            verts.push_back(Vector2f( 0.0f, -0.2f));
            verts.push_back(Vector2f( 1.5f,  1.8f));
            verts.push_back(Vector2f( 3.0f, -0.6f));

            uint idx[] = {0,1, 1,2, 2,3, 3,4, 4,5, 5,6};

            WallCreateInfo wci;
            wci.vertices = verts.data();
            wci.vertexCount = (uint)verts.size();
            wci.indices = idx;
            wci.indexCount = sizeof(idx)/sizeof(idx[0]);
            wci.thickness = 0.18f;
            wci.height = 2.2f;
            wci.cornerJoin = WallCreateInfo::CornerJoin::Round;
            wci.uv_tile_v = 1.5f;
            wci.uv_u_repeat_per_unit = 0.8f;

            Primitive *prim = CreateWallsFromLines2D(pc, &wci);
            if(prim)
            {
                SubMesh *mesh = CreateSubMesh(prim, material_instance, pipeline);
                if(mesh) wall_meshes.push_back(mesh);
            }
        }

        // 2) small rectangle loop (closed)
        {
            std::vector<Vector2f> verts;
            verts.push_back(Vector2f(-2.5f, -3.0f));
            verts.push_back(Vector2f(-2.5f, -1.0f));
            verts.push_back(Vector2f(-0.5f, -1.0f));
            verts.push_back(Vector2f(-0.5f, -3.0f));

            // indices pairs; make it closed by adding last first pair
            uint idx[] = {0,1, 1,2, 2,3, 3,0};

            WallCreateInfo wci;
            wci.vertices = verts.data();
            wci.vertexCount = (uint)verts.size();
            wci.indices = idx;
            wci.indexCount = sizeof(idx)/sizeof(idx[0]);
            wci.thickness = 0.25f;
            wci.height = 1.6f;
            wci.cornerJoin = WallCreateInfo::CornerJoin::Miter;
            wci.uv_tile_v = 1.0f;
            wci.uv_u_repeat_per_unit = 1.5f;

            Primitive *prim = CreateWallsFromLines2D(pc, &wci);
            if(prim)
            {
                SubMesh *mesh = CreateSubMesh(prim, material_instance, pipeline);
                if(mesh) wall_meshes.push_back(mesh);
            }
        }

        // 3) U-shaped wall
        {
            std::vector<Vector2f> verts;
            verts.push_back(Vector2f(1.0f, -2.5f));
            verts.push_back(Vector2f(1.0f, -0.5f));
            verts.push_back(Vector2f(3.0f, -0.5f));
            verts.push_back(Vector2f(3.0f, -2.5f));

            uint idx[] = {0,1, 1,2, 2,3}; // open U-shape

            WallCreateInfo wci;
            wci.vertices = verts.data();
            wci.vertexCount = (uint)verts.size();
            wci.indices = idx;
            wci.indexCount = sizeof(idx)/sizeof(idx[0]);
            wci.thickness = 0.22f;
            wci.height = 2.5f;
            wci.cornerJoin = WallCreateInfo::CornerJoin::Round;
            wci.uv_tile_v = 2.0f;
            wci.uv_u_repeat_per_unit = 0.6f;

            Primitive *prim = CreateWallsFromLines2D(pc, &wci);
            if(prim)
            {
                SubMesh *mesh = CreateSubMesh(prim, material_instance, pipeline);
                if(mesh) wall_meshes.push_back(mesh);
            }
        }

        // 4) irregular polyline cluster
        {
            std::vector<Vector2f> verts;
            verts.push_back(Vector2f(4.5f, 0.5f));
            verts.push_back(Vector2f(5.2f, 1.8f));
            verts.push_back(Vector2f(6.0f, 0.9f));
            verts.push_back(Vector2f(6.8f, 2.2f));
            verts.push_back(Vector2f(7.5f, 0.3f));

            uint idx[] = {0,1, 1,2, 2,3, 3,4};

            WallCreateInfo wci;
            wci.vertices = verts.data();
            wci.vertexCount = (uint)verts.size();
            wci.indices = idx;
            wci.indexCount = sizeof(idx)/sizeof(idx[0]);
            wci.thickness = 0.12f;
            wci.height = 1.8f;
            wci.cornerJoin = WallCreateInfo::CornerJoin::Bevel;
            wci.uv_tile_v = 1.0f;
            wci.uv_u_repeat_per_unit = 1.2f;

            Primitive *prim = CreateWallsFromLines2D(pc, &wci);
            if(prim)
            {
                SubMesh *mesh = CreateSubMesh(prim, material_instance, pipeline);
                if(mesh) wall_meshes.push_back(mesh);
            }
        }

        // Create components for each mesh
        CreateComponentInfo cci(GetSceneRoot());
        for(auto mesh: wall_meshes)
        {
            MeshComponentData *mcd = new MeshComponentData(mesh);
            ComponentDataPtr cdp = mcd;
            MeshComponent *mc = CreateComponent<MeshComponent>(&cci, cdp);
            if(mc) wall_components.push_back(mc);
        }

        delete pc;

        CameraControl *cc = GetCameraControl();
        cc->SetPosition(Vector3f(10,8,10));
        cc->SetTarget(Vector3f(0,0,0));

        return true;
    }
};

int os_main(int,os_char **)
{
    return RunFramework<TestApp>(OS_TEXT("Walls From Polyline Example - Complex"), 1280, 720);
}
