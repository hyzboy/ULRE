// ExtrudedPolygonTest.cpp
// 测试2D多边形挤压为3D多边形功能

#include<hgl/WorkManager.h>
#include<hgl/graph/geometry/ExtrudedPolygon.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/color/Color.h>
#include<hgl/component/MeshComponent.h>
#include<cmath>

using namespace hgl;
using namespace hgl::graph;

class ExtrudedPolygonTestApp : public WorkObject
{
private:

    Material *          material            = nullptr;
    Pipeline *          pipeline            = nullptr;

    Primitive *         prim_rect_cube      = nullptr;
    Primitive *         prim_circle_cylinder = nullptr;
    Primitive *         prim_triangle       = nullptr;
    Primitive *         prim_pentagon       = nullptr;
    MaterialInstance *  material_instance   = nullptr;

private:

    bool InitMDP()
    {
        mtl::Material3DCreateConfig cfg(PrimitiveType::Triangles);

        mtl::MaterialCreateInfo *mci=mtl::CreateGizmo3D(GetDevAttr(),&cfg);

        if(!mci)
            return(false);

        material=CreateMaterial("Gizmo3D",mci);

        Color4f color=GetColor4f(COLOR::BlenderAxisRed);

        material_instance = CreateMaterialInstance(material,(VIL *)nullptr,&color);

        pipeline = CreatePipeline(material_instance, InlinePipeline::Solid3D);

        return pipeline != nullptr;
    }

    bool CreateRenderObjects()
    {
        using namespace inline_geometry;

        auto pc = GetPrimitiveCreater(material_instance);

        // 测试1: 矩形挤压成立方体
        prim_rect_cube = CreateExtrudedRectangle(pc, 2.0f, 1.5f, 1.0f, Vector3f(0, 0, 1));

        // 测试2: 圆形挤压成圆柱体
        prim_circle_cylinder = CreateExtrudedCircle(pc, 0.8f, 1.5f, 16, Vector3f(0, 0, 1));

        // 测试3: 三角形挤压
        Vector2f triangleVertices[3] =
        {
            {-0.8f, -0.5f},  // 左下
            { 0.8f, -0.5f},  // 右下
            { 0.0f,  0.8f}   // 顶部
        };

        ExtrudedPolygonCreateInfo triangleEpci;
        triangleEpci.vertices = triangleVertices;
        triangleEpci.vertexCount = 3;
        triangleEpci.extrudeDistance = 1.2f;
        triangleEpci.extrudeAxis = Vector3f(0, 0, 1);
        triangleEpci.generateCaps = true;
        triangleEpci.generateSides = true;
        triangleEpci.clockwiseFront = true;

        prim_triangle = CreateExtrudedPolygon(pc, &triangleEpci);

        // 测试4: 五边形挤压
        Vector2f pentagonVertices[5];
        float angleStep = 2.0f * HGL_PI / 5.0f;

        for (int i = 0; i < 5; i++)
        {
            float angle = i * angleStep;
            pentagonVertices[i].x = cos(angle) * 0.7f;
            pentagonVertices[i].y = sin(angle) * 0.7f;
        }

        ExtrudedPolygonCreateInfo pentagonEpci;
        pentagonEpci.vertices = pentagonVertices;
        pentagonEpci.vertexCount = 5;
        pentagonEpci.extrudeDistance = 1.0f;
        pentagonEpci.extrudeAxis = Vector3f(1, 0, 0);  // X轴方向挤压
        pentagonEpci.generateCaps = true;
        pentagonEpci.generateSides = true;
        pentagonEpci.clockwiseFront = true;

        prim_pentagon = CreateExtrudedPolygon(pc, &pentagonEpci);

        return prim_rect_cube && prim_circle_cylinder && prim_triangle && prim_pentagon;
    }

    bool InitScene()
    {
        CreateComponentInfo cci(GetSceneRoot());

        // 创建矩形立方体网格 (位置: 原点)
        if (prim_rect_cube) {

            cci.mat=TranslateMatrix(-3,0,0);

            Mesh *mesh_rect = CreateMesh(prim_rect_cube, material_instance, pipeline);
            auto comp_rect = CreateComponent<MeshComponent>(&cci, mesh_rect);
        }

        // 创建圆柱体网格 (位置: 右侧)
        if (prim_circle_cylinder) {

            cci.mat=TranslateMatrix(3,0,0);

            Mesh *mesh_cylinder = CreateMesh(prim_circle_cylinder, material_instance, pipeline);
            auto comp_cylinder = CreateComponent<MeshComponent>(&cci, mesh_cylinder);
        }

        // 创建三角形柱网格 (位置: 前方)
        if (prim_triangle) {

            cci.mat=TranslateMatrix(0,3,0);

            Mesh *mesh_triangle = CreateMesh(prim_triangle, material_instance, pipeline);
            auto comp_triangle = CreateComponent<MeshComponent>(&cci, mesh_triangle);
        }

        // 创建五边形柱网格 (位置: 后方)
        if (prim_pentagon) {

            cci.mat=TranslateMatrix(0,-3,0);

            Mesh *mesh_pentagon = CreateMesh(prim_pentagon, material_instance, pipeline);
            auto comp_pentagon = CreateComponent<MeshComponent>(&cci, mesh_pentagon);
        }

        CameraControl *camera_control = GetCameraControl();
        camera_control->SetPosition(Vector3f(8, 8, 8));
        camera_control->SetTarget(Vector3f(0, 0, 0));

        return true;
    }

public:
    using WorkObject::WorkObject;

    ~ExtrudedPolygonTestApp()
    {
        SAFE_CLEAR(prim_rect_cube);
        SAFE_CLEAR(prim_circle_cylinder);
        SAFE_CLEAR(prim_triangle);
        SAFE_CLEAR(prim_pentagon);
    }

    bool Init() override
    {
        if (!InitMDP())
            return false;

        if (!CreateRenderObjects())
            return false;

        if (!InitScene())
            return false;

        return true;
    }
};

int os_main(int, os_char **)
{
    return RunFramework<ExtrudedPolygonTestApp>(OS_TEXT("Extruded Polygon"),1280,720);
}
