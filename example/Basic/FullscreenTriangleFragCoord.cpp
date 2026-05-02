#include <hgl/framework/WorkManager.h>
#include <hgl/graph/module/GeometryManager.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/Entity.h>
#include <hgl/ecs/components/TransformComponent.h>
#include <hgl/ecs/components/PrimitiveComponent.h>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

constexpr uint32_t VERTEX_COUNT = 3;

class TestApp : public WorkObject
{
private:

    ECSContext *ecs_world = nullptr;
    Entity *triangle_entity = nullptr;
    Geometry *geometry = nullptr;

    inline static const mtl::MaterialRecipe kCfg {
        .id       = "fullscreen_triangle_fragcoord",
        .preset   = mtl::MaterialPreset::FullscreenTriangle,
        .dim      = mtl::MaterialRecipe::Dim::D3,
        .l2w      = false,
        .pipeline = GraphicsPipelinePreset::Solid3D,
        .use_mesh_shader = true,
    };

private:

    bool CreateGeometryObject()
    {
        // FullscreenTriangle uses PCG_FullscreenTriangle position provider.
        // Geometry only carries vertex_count (=3) and does not upload any VABs.
        geometry = CreateGeometry("FullscreenTrianglePCG",
                                  VERTEX_COUNT,
                                  0,
                                  IndexType::AUTO,
                                  {},
                                  nullptr);

        return geometry != nullptr;
    }

    bool InitECS()
    {
        ecs_world = GetECSContext();
        if (!ecs_world)
            return false;

        triangle_entity = ecs_world->CreateEntity<Entity>("FullscreenTriangle");

        auto transform = triangle_entity->AddComponent<TransformComponent>(Mobility::Static);
        transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        transform->SetMovable(false);

        auto primitive = triangle_entity->AddComponent<hgl::ecs::PrimitiveComponent>();
        primitive->SetUnresolvedGeometry(geometry);
        primitive->SetMaterialRecipe(RegisterMaterialRecipe(kCfg));
        primitive->SetVisible(true);

        return true;
    }

public:

    bool Init() override
    {
        SetClearColor(Color4f(0.05f, 0.05f, 0.05f, 1.0f));

        if (!CreateGeometryObject())
            return false;

        if (!InitECS())
            return false;

        return true;
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<TestApp>(OS_TEXT("Fullscreen Triangle FragCoord"), argc, argv);
}
