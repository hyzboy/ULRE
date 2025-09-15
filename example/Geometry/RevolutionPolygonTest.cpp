// RevolutionPolygonTest.cpp
// 通过旋转2D线条生成3D几何体的示例

#include<hgl/WorkManager.h>
#include<hgl/graph/geo/Revolution.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/color/Color.h>
#include<hgl/component/MeshComponent.h>
#include<cmath>

using namespace hgl;
using namespace hgl::graph;

class RevolutionPolygonTestApp : public WorkObject
{
private:

    Material *          material            = nullptr;
    Pipeline *          pipeline            = nullptr;

    Primitive *         prim_vase           = nullptr;
    Primitive *         prim_partial_shell  = nullptr;
    Primitive *         prim_cone           = nullptr;
    MaterialInstance *  material_instance   = nullptr;

private:
    bool InitMDP()
    {
        mtl::Material3DCreateConfig cfg(PrimitiveType::Triangles);

        mtl::MaterialCreateInfo *mci = mtl::CreateGizmo3D(GetDevAttr(), &cfg);
        if(!mci)
            return false;

        material = CreateMaterial("Gizmo3D", mci);

        Color4f color = GetColor4f(COLOR::BlenderAxisRed);

        material_instance = CreateMaterialInstance(material, (VIL *)nullptr, &color);

        pipeline = CreatePipeline(material_instance, InlinePipeline::Solid3D);

        return pipeline != nullptr;
    }

    bool CreateRenderObjects()
    {
        using namespace inline_geometry;

        // 取得 PrimitiveCreater
        auto pc = GetPrimitiveCreater(material_instance);
        if(!pc) return false;

        // 示例1: 花瓶型 (完整旋转 360)
        Vector2f vaseProfile[] = {
            {0.0f, -1.0f},
            {0.2f, -0.8f},
            {0.5f, -0.3f},
            {0.6f,  0.3f},
            {0.4f,  0.8f},
            {0.2f,  1.0f}
        };

        RevolutionCreateInfo vaseRci;
        vaseRci.profile_points = vaseProfile;
        vaseRci.profile_count = sizeof(vaseProfile)/sizeof(Vector2f);
        vaseRci.revolution_slices = 36;
        vaseRci.start_angle = 0.0f;
        vaseRci.sweep_angle = 360.0f;
        vaseRci.revolution_center = Vector3f(0,0,0);
        vaseRci.close_profile = false;
        vaseRci.uv_scale = Vector2f(1.0f,1.0f);

        prim_vase = CreateRevolution(pc, &vaseRci);

        // 示例2: 半旋转的薄壳（展示端面）
        Vector2f shellProfile[] = {
            {0.0f, 0.0f},
            {0.8f, 0.0f},
            {0.8f, 1.0f},
            {0.0f, 1.0f}
        };

        RevolutionCreateInfo shellRci;
        shellRci.profile_points = shellProfile;
        shellRci.profile_count = sizeof(shellProfile)/sizeof(Vector2f);
        shellRci.revolution_slices = 24;
        shellRci.start_angle = -90.0f;
        shellRci.sweep_angle = 180.0f; // 半旋转
        shellRci.revolution_center = Vector3f(0,0,0);
        shellRci.close_profile = true; // 闭合轮廓
        shellRci.uv_scale = Vector2f(1.0f,1.0f);

        prim_partial_shell = CreateRevolution(pc, &shellRci);

        // 示例3: 简单圆锥（使用两点轮廓）
        Vector2f coneProfile[] = {
            {0.0f, 0.0f},
            {0.6f, 1.5f}
        };

        RevolutionCreateInfo coneRci;
        coneRci.profile_points = coneProfile;
        coneRci.profile_count = 2;
        coneRci.revolution_slices = 32;
        coneRci.start_angle = 0.0f;
        coneRci.sweep_angle = 360.0f;
        coneRci.revolution_center = Vector3f(0,0,0);
        coneRci.close_profile = true;
        coneRci.uv_scale = Vector2f(1.0f,1.0f);

        prim_cone = CreateRevolution(pc, &coneRci);

        return prim_vase && prim_partial_shell && prim_cone;
    }

    bool InitScene()
    {
        CreateComponentInfo cci(GetSceneRoot());

        if(prim_vase) {
            cci.mat = TranslateMatrix(-3, 0, 0);
            Mesh *mesh = CreateMesh(prim_vase, material_instance, pipeline);
            auto comp = CreateComponent<MeshComponent>(&cci, mesh);
        }

        if(prim_partial_shell) {
            cci.mat = TranslateMatrix(3, 0, 0);
            Mesh *mesh = CreateMesh(prim_partial_shell, material_instance, pipeline);
            auto comp = CreateComponent<MeshComponent>(&cci, mesh);
        }

        if(prim_cone) {
            cci.mat = TranslateMatrix(0, 3, 0);
            Mesh *mesh = CreateMesh(prim_cone, material_instance, pipeline);
            auto comp = CreateComponent<MeshComponent>(&cci, mesh);
        }

        CameraControl *camera_control = GetCameraControl();
        camera_control->SetPosition(Vector3f(8,8,8));
        camera_control->SetTarget(Vector3f(0,0,0));

        return true;
    }

public:
    using WorkObject::WorkObject;

    ~RevolutionPolygonTestApp()
    {
        SAFE_CLEAR(prim_vase);
        SAFE_CLEAR(prim_partial_shell);
        SAFE_CLEAR(prim_cone);
    }

    bool Init() override
    {
        if(!InitMDP()) return false;
        if(!CreateRenderObjects()) return false;
        if(!InitScene()) return false;
        return true;
    }
};

int os_main(int, os_char **)
{
    return RunFramework<RevolutionPolygonTestApp>(OS_TEXT("Revolution Polygon"), 1280, 720);
}
