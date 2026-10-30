#include <hgl/framework/WorkManager.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/asset/SceneAssetRegistry.h>
#include <hgl/graph/asset/SceneLoader.h>
#include <hgl/graph/geo/GeometryCreater.h>
#include <hgl/graph/module/BufferManager.h>
#include <hgl/graph/module/GeometryManager.h>
#include <hgl/type/Smart.h>
#include <hgl/vk/VertexDataManager.h>
#include <hgl/ecs/core/Context.h>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{
    GeometryVertexFormat CreateStandardGeometryVertexFormat()
    {
        return GeometryVertexFormat{
            {VertexSemantic::Position, VF_V3F},
            {VertexSemantic::TexCoord, VF_V2HF},
            {VertexSemantic::Normal, VF_V2UN8},
        };
    }
}

class BasicLitMeshesApp : public WorkObject
{
private:
    ECSContext *ecs_context = nullptr;
    AutoDelete<VertexDataManager> mesh_vdm;
    SceneInstance scene_instance;

    bool InitVDM()
    {
        auto *buffer_manager = GetManager<BufferManager>();
        if (!buffer_manager)
            return false;

        mesh_vdm = new VertexDataManager(
            buffer_manager,
            CreateStandardGeometryVertexFormat());
        return mesh_vdm && mesh_vdm->Init(
            HGL_SIZE_1MB,
            HGL_SIZE_1MB,
            IndexType::U32);
    }

    bool InitScene()
    {
        ecs_context = GetECSContext();
        if (!ecs_context || !InitVDM())
            return false;

        GraphicsContext *graphics = ecs_context->GetGraphicsContext();
        SceneAssetRegistry *scene_registry =
            graphics ? graphics->GetSceneAssetRegistry() : nullptr;
        if (!scene_registry)
            return false;

        SceneAsset *scene = scene_registry->GetOrLoad(
            OS_TEXT("example/Material/BasicLitMeshes.scene.toml"));
        if (!scene)
            return false;

        GeometryCreater creater(mesh_vdm);
        return scene->Instantiate(ecs_context, &creater, scene_instance);
    }

public:
    ~BasicLitMeshesApp()
    {
        scene_instance.Release();
        if (auto *geometry_manager = GetManager<GeometryManager>())
            geometry_manager->Release();
        mesh_vdm = nullptr;
    }

    bool Init() override
    {
        return InitScene();
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<BasicLitMeshesApp>(
        OS_TEXT("Standard Meshes"),
        argc,
        argv,
        1280,
        720);
}
