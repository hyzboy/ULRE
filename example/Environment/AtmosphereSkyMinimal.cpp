#include<hgl/framework/WorkManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/ShaderBufferSources.h>
#include<hgl/graph/module/GeometryManager.h>
#include<memory>

// ECS headers
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/support/TransformAccessor.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/components/CameraControlMode.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/EnvironmentSystem.h>
#include<hgl/vk/VKRenderTargetSwapchain.h>
#include "../Shared/FrameDigest.h"   // 帧摘要自检（T0 判据前置共用件）

using namespace hgl;
using namespace hgl::graph;

namespace
{
    GeometryVertexFormat CreateSkyMinimalGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V3F},
        };
        return gvf;
    }
}

class AtmosphereSkyMinimalApp:public WorkObject
{
private:

    hgl::ecs::ECSContext *ecs_context = nullptr;
    hgl::ecs::Entity *sky_entity = nullptr;
    hgl::ecs::Entity *camera_entity = nullptr;

    Geometry *          prim_sky_sphere     =nullptr;
    graph::mtl::MaterialRecipe sky_recipe{};
    PrimitiveAsset             sky_asset{};

private:

    bool InitRecipe()
    {
        if (!prim_sky_sphere)
            return false;
        sky_recipe.recipe_name = "AtmosphereSkyMinimal.Sky";
        sky_recipe.mtl_def_id = "SkyMinimal";
        sky_recipe.render_state_overrides.pipeline_config = mtl::MakeSkyConfig();
        sky_asset = PrimitiveAsset(prim_sky_sphere, &sky_recipe, PrimitiveType::Triangles);
        return true;
    }

    bool CreateRenderObject()
    {
        auto* device = GetDevice();
        auto* geometry_manager = GetManager<GeometryManager>();
        if (!device || !geometry_manager)
            return false;

        using namespace inline_geometry;

        auto pc = std::make_unique<GeometryCreater>(
            device,
            CreateSkyMinimalGeometryVertexFormat());

        struct HexSphereCreateInfo hsci;

        hsci.subdivisions=3;
        hsci.radius=256;

        prim_sky_sphere=CreateHexSphere(pc.get(),&hsci);
        if (prim_sky_sphere)
            geometry_manager->Add(prim_sky_sphere);

        return prim_sky_sphere;
    }

    bool InitECSScene()
    {
        if(!ecs_context)
            return false;

        if(!prim_sky_sphere)
            return false;

        sky_entity = ecs_context->CreateEntity<hgl::ecs::Entity>("SkySphere");
        auto transform = ecs_context->GetTransform(ecs_context->CreateTransform(sky_entity->GetEntityID(), hgl::ecs::Mobility::Movable));
        auto prim_comp = sky_entity->GetContext()->GetOrCreateGeometryData(sky_entity);

        transform.SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        transform.SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform.SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        transform.SetMobility(hgl::ecs::Mobility::Static);

        prim_comp->GetOwner()->GetContext()->GetOrCreateGeometryData(prim_comp->GetOwnerID())->SetPrimitiveAsset(&sky_asset);
        // [A5a] 可见性真值已收敛到实体级（默认即可见）：原组件级 SetVisible(true) 等义调用已删

        return true;
    }

    bool InitCamera()
    {
        if (!ecs_context || !ecs_context->EnsureCameraSystem())
            return false;

        camera_entity = ecs_context->CreateEntity<hgl::ecs::Entity>("MainCamera");
        // 相机 = **世界级资源**：经世界访问器创建/取回（实体只是宿主），不直取组件
        auto *camera = ecs_context->GetOrCreateCamera(camera_entity);

        camera->control_mode = hgl::ecs::CameraControlMode::ViewModel;
        camera->target = math::Vector3f(10,10,10);
        camera->distance = 1.0f;
        camera->yaw = 0.0f;
        camera->pitch = 0.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->viewport_info = GetViewportInfo();

        return true;
    }

    bool InitECS()
    {
        ecs_context = GetECSContext();
        if(!ecs_context)
            return false;

        auto environment_system = ecs_context->GetSystem<hgl::ecs::EnvironmentSystem>();
        if (!environment_system)
            environment_system = ecs_context->RegisterRenderSystem<hgl::ecs::EnvironmentSystem>();

        if (environment_system)
        {
            environment_system->EditSkyInfo();
                    }

        if(!InitECSScene())
            return false;

        if(!InitCamera())
            return false;

        return true;
    }

public:
    void Tick(double delta_time) override
    {
        WorkObject::Tick(delta_time);

        // SKYMIN_SELFCHECK=1：**帧摘要自检**（T0 判据前置；共用件 example/Shared/FrameDigest.h）——
        // 第 8 帧帧内回读颜色靶 ⇒ {不同字节取值数, FNV-1a 64} 比对下方内联常量：PASS 退出 0 / FAIL 退出 1。
        // 为什么本例必须有它：天空球自身外观（sky_provider 参数块）在八台架里只经默认模板的 ambient
        // 间接参与 ⇒ 结构搬家时该路径只有这条判据有分辨力。
        // 判据只建在**冻结相机 + 定帧**上；常量由实跑记录，改渲染结果（少画/错画/挪相机）必须复核重录。
        static const bool s_selfcheck = (getenv("SKYMIN_SELFCHECK") != nullptr);
        static uint32_t   s_self_frame = 0;

        static constexpr uint32_t kSelfW           = 1280;
        static constexpr uint32_t kSelfH           = 720;
        static constexpr uint32_t kSelfMinDistinct = 219;   // 不空转守卫（恒定图会 FAIL）
        static constexpr uint64_t kSelfFNV         = 0xC914C338EFC7790Eull;  // T0 实跑记录

        if (s_selfcheck && ++s_self_frame == 8)
        {
            auto * rt = dynamic_cast<graph::SwapchainRenderTarget *>( ecs_context ? ecs_context->GetRenderTarget() : nullptr );
            if (!rt)
            {
                printf("[SKYSELF] no swapchain RT\n");
                std::exit(2);
            }

            ulre_example::FrameDigest expect;
            expect.width                = kSelfW;
            expect.height               = kSelfH;
            expect.distinct_byte_values = kSelfMinDistinct;
            expect.fnv1a64              = kSelfFNV;

            ulre_example::InstallFrameDigestHook(rt, expect, "SKYSELF");
        }
    }

    bool Init() override
    {
        if(!CreateRenderObject())
            return(false);

        if(!InitRecipe())
            return(false);

        if(!InitECS())
            return(false);

        return(true);
    }
};//class AtmosphereSkyMinimalApp:public CameraAppFramework

int os_main(int argc,os_char **argv)
{
    return RunFramework<AtmosphereSkyMinimalApp>(OS_TEXT("SimplestAtmosphere"),argc,argv,1280,720);
}
