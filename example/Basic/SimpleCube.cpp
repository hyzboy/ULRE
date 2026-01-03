#include<hgl/WorkManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/component/PrimitiveComponent.h>
#include<hgl/color/Color.h>

using namespace hgl;
using namespace hgl::graph;

class TestApp:public WorkObject
{
private:

    Material *          material        = nullptr;
    MaterialInstance *  mi              = nullptr;
    Pipeline *          pipeline        = nullptr;

    Geometry *          geometry        = nullptr;
    Primitive *         primitive       = nullptr;

private:

    bool InitMaterial()
    {
        mtl::Material3DCreateConfig cfg(PrimitiveType::Triangles);

        mtl::MaterialCreateInfo *mci = mtl::CreateGizmo3D(GetDevAttr(), &cfg);

        if(!mci)
            return false;

        material = CreateMaterial("Gizmo3D", mci);

        if(!material)
            return false;

        Color4f color = GetColor4f(COLOR::BlenderAxisBlue, 1.0f);

        mi = CreateMaterialInstance(material, (VIL *)nullptr, &color);

        if(!mi)
            return false;

        pipeline = CreatePipeline(material, InlinePipeline::Solid3D);

        return pipeline != nullptr;
    }

    bool CreateCubeGeometry()
    {
        using namespace inline_geometry;

        auto pc = GetGeometryCreater(material);

        if(!pc)
            return false;

        CubeCreateInfo cci;
        cci.segments_x = 2;
        cci.segments_y = 3;
        cci.segments_z = 4;

        geometry = CreateCube(pc, &cci);

        return geometry != nullptr;
    }

    bool InitScene()
    {
        primitive = CreatePrimitive(geometry, mi, pipeline);

        if(!primitive)
            return false;

        CreateComponentInfo cci(GetWorldRootNode());

        if(!CreateComponent<PrimitiveComponent>(&cci, primitive))
            return false;

        CameraControl *camera_control = GetCameraControl();

        camera_control->SetPosition(math::Vector3f(2, 2, 2));
        camera_control->SetTarget(math::Vector3f(0, 0, 0));

        return true;
    }

public:

    using WorkObject::WorkObject;

    ~TestApp()
    {
        SAFE_CLEAR(primitive)
        SAFE_CLEAR(geometry)
    }

    bool Init() override
    {
        if(!InitMaterial())
            return false;

        if(!CreateCubeGeometry())
            return false;

        if(!InitScene())
            return false;

        return true;
    }
};

int os_main(int, os_char **)
{
    return RunFramework<TestApp>(OS_TEXT("Simple Cube"), 1280, 720);
}
