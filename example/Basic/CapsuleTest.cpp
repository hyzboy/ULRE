// CapsuleTest
// 测试胶囊体几何生成器，包括实体版本和线框版本

#include<hgl/WorkManager.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/InlineGeometry.h>
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

class TestApp:public WorkObject
{
private:

    Material *          material_solid      =nullptr;
    Material *          material_wireframe  =nullptr;
    Pipeline *          pipeline_solid      =nullptr;
    Pipeline *          pipeline_wireframe  =nullptr;

    Primitive *         prim_capsule_solid     =nullptr;
    Primitive *         prim_capsule_wireframe =nullptr;
    MaterialInstance *  material_instance_solid    =nullptr;
    MaterialInstance *  material_instance_wireframe=nullptr;

private:

    bool InitMDP()
    {
        // 实体胶囊
        mtl::Material3DCreateConfig cfg_solid(PrimitiveType::Triangles);
        cfg_solid.local_to_world=true;
        cfg_solid.position=true;
        cfg_solid.normal=true;
        material_instance_solid=CreateMaterialInstance(mtl::inline_material::FlatColor3D,&cfg_solid);
        pipeline_solid=CreatePipeline(material_instance_solid,InlinePipeline::Solid3D);

        // 线框胶囊
        mtl::Material3DCreateConfig cfg_wireframe(PrimitiveType::Lines);
        cfg_wireframe.local_to_world=true;
        cfg_wireframe.position=true;
        material_instance_wireframe=CreateMaterialInstance(mtl::inline_material::VertexColor3D,&cfg_wireframe);
        pipeline_wireframe=CreatePipeline(material_instance_wireframe,InlinePipeline::Solid3D);

        return pipeline_solid && pipeline_wireframe;
    }

    bool CreateRenderObject()
    {
        using namespace inline_geometry;

        // 创建实体胶囊
        auto pc_solid = GetPrimitiveCreater(material_instance_solid);
        CapsuleCreateInfo cci;
        cci.halfExtend = 2.0f;      // 圆柱部分高度的一半
        cci.radius = 1.0f;          // 半径
        cci.numberSlices = 16;      // 圆切分精度
        cci.numberStacks = 8;       // 球帽层数

        prim_capsule_solid = CreateCapsule(pc_solid, &cci);

        // 创建线框胶囊
        auto pc_wireframe = GetPrimitiveCreater(material_instance_wireframe);
        prim_capsule_wireframe = CreateCapsuleWireframe(pc_wireframe, &cci);

        return prim_capsule_solid && prim_capsule_wireframe;
    }

    bool InitScene()
    {
        // 添加实体胶囊
        Mesh *mesh_solid = CreateMesh(prim_capsule_solid, material_instance_solid, pipeline_solid);
        CreateComponentInfo cci_solid(GetSceneRoot());
        CreateComponent<MeshComponent>(&cci_solid, mesh_solid);

        // 添加线框胶囊 (偏移一点位置)
        Mesh *mesh_wireframe = CreateMesh(prim_capsule_wireframe, material_instance_wireframe, pipeline_wireframe);
        CreateComponentInfo cci_wireframe(GetSceneRoot());
        auto wireframe_comp = CreateComponent<MeshComponent>(&cci_wireframe, mesh_wireframe);
        wireframe_comp->SetTranslate(Vector3f(5.0f, 0.0f, 0.0f)); // 向右偏移

        CameraControl *camera_control = GetCameraControl();
        camera_control->SetPosition(Vector3f(10, 8, 8));
        camera_control->SetTarget(Vector3f(2.5, 0, 0));

        return true;
    }

public:

    using WorkObject::WorkObject;

    ~TestApp()
    {
        SAFE_CLEAR(prim_capsule_solid);
        SAFE_CLEAR(prim_capsule_wireframe);
    }

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
};//class TestApp:public WorkObject

int os_main(int,os_char **)
{
    return RunFramework<TestApp>(OS_TEXT("CapsuleTest"),1280,720);
}