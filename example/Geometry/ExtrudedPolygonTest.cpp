// ExtrudedPolygonTest.cpp
// 测试2D多边形挤压为3D多边形功能

#include<hgl/WorkManager.h>
#include<hgl/graph/geo/Extruded.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/color/Color.h>
#include<hgl/component/PrimitiveComponent.h>
#include<cmath>

using namespace hgl;
using namespace hgl::graph;

class ExtrudedPolygonTestApp : public WorkObject
{
private:

    Material *          material            = nullptr;
    Pipeline *          pipeline            = nullptr;

    Geometry *         prim_rect_cube      = nullptr;
    Geometry *         prim_circle_cylinder = nullptr;
    Geometry *         prim_triangle       = nullptr;
    Geometry *         prim_pentagon       = nullptr;
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

        auto pc = GetGeometryCreater(material_instance);

        // 测试1: 矩形挤压成立方体
        prim_rect_cube = CreateExtrudedRectangle(pc, 2.0f, 1.5f, 1.0f, math::Vector3f(0, 0, 1));

        // 测试2: 圆形挤压成圆柱体
        prim_circle_cylinder = CreateExtrudedCircle(pc, 0.8f, 1.5f, 16, math::Vector3f(0, 0, 1));

        // 测试3: 三角形挤压
        math::Vector2f triangleVertices[3] =
        {
            {-0.8f, -0.5f},  // 左下
            { 0.8f, -0.5f},  // 右下
            { 0.0f,  0.8f}   // 顶部
        };

        ExtrudedPolygonCreateInfo triangleEpci;
        triangleEpci.vertices = triangleVertices;
        triangleEpci.vertexCount = 3;
        triangleEpci.extrudeDistance = 1.2f;
        triangleEpci.extrudeAxis = math::Vector3f(0, 0, 1);
        triangleEpci.generateCaps = true;
        triangleEpci.generateSides = true;
        triangleEpci.clockwiseFront = true;

        prim_triangle = CreateExtrudedPolygon(pc, &triangleEpci);

        // 测试4: 五边形挤压
        math::Vector2f pentagonVertices[5];
        float angleStep = 2.0f * math::pi / 5.0f;

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
        pentagonEpci.extrudeAxis = math::Vector3f(1, 0, 0);  // X轴方向挤压
        pentagonEpci.generateCaps = true;
        pentagonEpci.generateSides = true;
        pentagonEpci.clockwiseFront = true;

        prim_pentagon = CreateExtrudedPolygon(pc, &pentagonEpci);

        return prim_rect_cube && prim_circle_cylinder && prim_triangle && prim_pentagon;
    }

    bool InitScene()
    {
        CreateComponentInfo cci(GetWorldRootNode());

        // 创建矩形立方体网格 (位置: 原点)
        if (prim_rect_cube) {

            cci.mat=math::TranslateMatrix(-3,0,0);

            Primitive *mesh_rect = CreatePrimitive(prim_rect_cube, material_instance, pipeline);
            auto comp_rect = CreateComponent<PrimitiveComponent>(&cci, mesh_rect);
        }

        // 创建圆柱体网格 (位置: 右侧)
        if (prim_circle_cylinder) {

            cci.mat=math::TranslateMatrix(3,0,0);

            Primitive *mesh_cylinder = CreatePrimitive(prim_circle_cylinder, material_instance, pipeline);
            auto comp_cylinder = CreateComponent<PrimitiveComponent>(&cci, mesh_cylinder);
        }

        // 创建三角形柱网格 (位置: 前方)
        if (prim_triangle) {

            cci.mat=math::TranslateMatrix(0,3,0);

            Primitive *mesh_triangle = CreatePrimitive(prim_triangle, material_instance, pipeline);
            auto comp_triangle = CreateComponent<PrimitiveComponent>(&cci, mesh_triangle);
        }

        // 创建五边形柱网格 (位置: 后方)
        if (prim_pentagon) {

            cci.mat=math::TranslateMatrix(0,-3,0);

            Primitive *mesh_pentagon = CreatePrimitive(prim_pentagon, material_instance, pipeline);
            auto comp_pentagon = CreateComponent<PrimitiveComponent>(&cci, mesh_pentagon);
        }

        CameraControl *camera_control = GetCameraControl();
        camera_control->SetPosition(math::Vector3f(8, 8, 8));
        camera_control->SetTarget(math::Vector3f(0, 0, 0));

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
