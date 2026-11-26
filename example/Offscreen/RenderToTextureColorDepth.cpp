// RenderToTextureColorDepth —— 离屏世界回归床（color + depth 双附件）
//
// 以 RenderToTexture 为基线，多一路深度显示：离屏世界渲染一帧（颜色 + 深度附件），
// 主画面并排两个立方体，左贴 `rt:<RT 名>:color0`，右贴 `rt:<RT 名>:depth`。
// 前端同样只有两句话：材质直接给纹理 URI；离屏世界每帧驱动到 `IsContentReady()`
// 为真即停。示例不碰裸纹理、不做同步预渲染、不管加载时机。
//
// env：
//   RTTCD_SELFCHECK=1   帧摘要自检（就绪后第 8 帧；PASS 退 0 / FAIL 退 1 / 读回失败退 2）
//   RTTCD_DUMP=<tag>    就绪后第 8 帧帧内回读主场景颜色靶落原始字节后退出
//                       （`rttcddump_<tag>_WxH_fmt.raw`）
//   RTTCD_CAPTURE_ON_REQUEST=1  抓取请求时提交离屏帧，供集成测试

#include<hgl/framework/WorkManager.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKOffscreenRenderTarget.h>
#include<hgl/vk/VKOffscreenRenderTarget.h>
#include<hgl/vk/DebugCaptureQueue.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/type/Smart.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/world/assembly/OffscreenWorld.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/ssbo/GlobalSSBOTypes.h>
#include<hgl/graph/ssbo/MaterialDataRows.h>

#include<hgl/graph/module/EnvironmentManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/color/Color.h>
#include<hgl/log/Log.h>
#include<hgl/mtl/MaterialRecipe.h>

#include<hgl/graph/ssbo/LitMaterialData.h>

#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/entity/Entity.h>
#include<hgl/ecs/support/transform/TransformAccessor.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/components/CameraControlMode.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/collect/RenderCollectSystem.h>
#include<hgl/ecs/systems/render/frame/RenderTargetSystem.h>
#include<hgl/ecs/systems/render/frame/RenderSystemCore.h>
#include<hgl/ecs/systems/render/light_shadow/LightSystem.h>
#include<hgl/ecs/systems/render/buffer/RenderSceneUBOSystem.h>
#include<hgl/ecs/systems/tick/InputSystem.h>

#include<hgl/vk/VKRenderTargetSwapchain.h>
#include "../Shared/ExampleAppBase.h"

#include <memory>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <numbers>

/**
 * 关于深度可视化（之前 ShadowMap 示例"什么都看不到"的根因）：
 *   引擎深度为逆序 Z（近端=1、远端=0，实测 d≈near/z）：透视下 d 对距离高度非线性，
 *   默认 near=0.1 时全部深度值都落在 0.01 量级，与清屏值 0 分不开，做成贴图是一片黑。
 *   本例把离屏相机的 near/far **收紧到刚好包住物体**（distance ± 球半径附近），
 *   让深度值铺满 [0,1]，深度图才有对比度。
 *
 *   另一个注意点：本例深度立方体走 `UnlitTexture3D`（世界空间无光贴面，直接输出
 *   采样值、不经光照）。采样深度时 `texture()` 只有一个通道有效，引擎在**采样视图**
 *   上把 R 广播到 RGB ⇒ 深度图呈灰度；若走 `Lit` 这类 PBR 材质，深度值会被当成
 *   反照率再过一遍光照，环境高光把整个面染成天空色，看上去和颜色立方体一模一样
 *   ——这正是「两个立方体都在显示 COLOR」的根因。
 *
 *   深度 RT 的空区域是清屏值（远端 0），映射到立方体面上即为黑色，与场景背景重合，
 *   所以深度立方体看起来只有一块「浮着的灰色圆盘」（球面深度）+ 淡淡的棱边，
 *   这是深度数据的正常表现，不是渲染错误。
 */

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{
    // 材质数据行的唯一来源：一个 URI + 一个行号，材质数值全部在块文件里
    constexpr char   MATERIAL_DATA_URI[] =
        "mtldata:pack(ShaderLibrary):asset/material_data/render_to_texture_color_depth";
    constexpr uint32 MATERIAL_DATA_ROW_SPHERE = 0;
    constexpr uint32 MATERIAL_DATA_ROW_CUBE   = 1;
}

namespace
{
    GeometryVertexFormat CreateStandardGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V3F},
            {VertexSemantic::TexCoord, VF_V2F},
            {VertexSemantic::Normal,   VF_V3F},
        };
        return gvf;
    }

    void LogTextureInfo(const char *tag, Texture2D *tex)
    {
        if (!tex)
        {
            GLogInfo("[RTTColorDepth] %s tex=null", tag);
            return;
        }

        GLogInfo("[RTTColorDepth] %s tex=%p image=%p view=%p layout=%u",
                tag,
                (void *)tex,
                (void *)tex->GetImage(),
                (void *)tex->GetVulkanImageView(),
                (uint32_t)tex->GetImageLayout());
    }

    bool LogStageFail(const char *stage, const char *reason)
    {
        GLogError("[RTTColorDepth][%s] %s", stage, reason);
        return false;
    }

    void LogStage(const char *stage, const char *message)
    {
        GLogInfo("[RTTColorDepth][%s] %s", stage, message);
    }

    const bool  s_capture_on_request = (getenv("RTTCD_CAPTURE_ON_REQUEST") != nullptr);
}

/**
 * 离屏 Pass：一帧同时产出颜色与深度
 *
 * 与 RenderToTexture 的差别只有两点：
 *   1. 对外同时暴露颜色纹理与深度纹理
 *   2. 离屏相机的 near/far 收紧，使深度值在 [0,1] 内有对比度（否则深度贴图是一片白）
 */
class OffscreenPass
{
private:
    std::unique_ptr<graph::OffscreenWorld> offscreen;

    RenderContext *render_context = nullptr;
    graph::EnvProfileID offscreen_env_profile = graph::kEnvProfileDefault;

    Geometry *geometry = nullptr;
    PrimitiveAsset sphere_asset;
    graph::mtl::MaterialRecipe sphere_recipe{};

    graph::GlobalSSBOBinding material_data_ssbo_accessor{};
    Entity *sphere_entity = nullptr;
    hgl::ecs::GeometryData *sphere_primitive_comp = nullptr;

    /// 离屏相机距目标的距离，决定 near/far 收紧范围
    static constexpr float kCameraDistance = 6.0f;

    /// 球体半径（inline_geometry::CreateSphere 生成的是单位球）
    static constexpr float kSphereRadius = 1.0f;

private:

    bool InitMaterialDataSSBO(WorkObject *owner)
    {
        if (!owner->GetMaterialDataBinding(MATERIAL_DATA_URI, MATERIAL_DATA_ROW_SPHERE,
                                          material_data_ssbo_accessor))
            return LogStageFail("OffscreenPass::InitMaterialDataSSBO", "material data block binding failed");

        LogStage("OffscreenPass::InitMaterialDataSSBO", "success");
        return true;
    }

public:
    ~OffscreenPass()
    {
        GraphicsContext *gc = render_context ? render_context->GetGraphicsContext() : nullptr;
        if (!gc)
        {
            if (auto *w = offscreen ? offscreen->GetWorld() : nullptr)
                gc = w->GetGraphicsContext();
        }

        if (gc)
        {
            if (geometry)
            {
                if (auto *gm = gc->GetGeometryManager())
                    gm->Release(geometry);
            }
        }

        geometry = nullptr;
    }

    /// 离屏内容已产出（内部图元资源齐备并画过一帧）
    bool IsContentReady() const
    {
        return offscreen && offscreen->IsContentReady();
    }

    bool IsSceneContentReady() const
    {
        return offscreen && offscreen->IsSceneContentReady();
    }

    IRenderTarget *GetRenderTarget() const
    {
        return offscreen ? offscreen->GetRenderTarget() : nullptr;
    }

    bool Init(WorkObject *owner, const uint32_t width, const uint32_t height)
    {
        GLogInfo("[RTTColorDepth][OffscreenPass::Init] begin owner=%p size=%ux%u",
                 (void *)owner, width, height);

        // 离屏世界：一行描述 + 一次 Create，RT / ECSContext / 渲染系统全部就位
        graph::OffscreenWorldDesc desc;
        desc.width  = width;
        desc.height = height;
        desc.name   = "RTTColorDepth_Offscreen";
        desc.resource_prefix = "RTTColorDepth:OffscreenRT";

        // 清屏色声明在 desc 里，成为 RT 上的权威值。
        // RTT 内容会被主场景 Lit 材质再乘一次光照（kd*NdotL/π ≈ 0.16），
        // 离屏用亮天蓝补偿，避免贴到立方体上整体发黑。
        desc.clear_color = GetColor4f(COLOR::LightSkyBlue, 1.0f);

        offscreen = graph::OffscreenWorld::Create(owner->GetGraphicsContext(),
                                                  owner->GetECSContext(),
                                                  desc);
        if (!offscreen)
            return LogStageFail("OffscreenPass::Init", "OffscreenWorld::Create failed");

        if(s_capture_on_request)
            if(auto *target=dynamic_cast<graph::OffscreenRenderTarget *>(offscreen->GetRenderTarget()))
                target->EnableManagedDebugCapture();

        // RTT 内容会被主场景 Lit 材质再乘一次光照，离屏用高太阳强度 profile 补偿
        if (auto *gc = owner->GetGraphicsContext())
        {
            if (auto *env_manager = gc->GetEnvironmentManager())
            {
                graph::EnvironmentInfo info{};

                // T6b：光源参数已迁到**世界私有光源行**（不再跟 profile）—— profile 里只剩天空外观
                // （底色 / 光晕强度；光晕色恒为主光色，故不需要也写入）。
                const graph::SunLightSample sun = graph::EvaluateSunLight(8.0f, 30.0f, 0.0f);

                offscreen_env_profile = env_manager->Create("RTTColorDepth.OffscreenBright", info);
                if (offscreen->GetRenderTarget())
                    offscreen->GetRenderTarget()->SetEnvironmentProfile(offscreen_env_profile);

                // 离屏世界的主光：8:30 的方向/颜色 + 强度 ×4。离屏世界没有 LightSystem
                // ⇒ 直接写它的**光源行**（行表归世界所有，不需要系统在场）。
                if (auto *world = offscreen->GetWorld())
                {
                    if (auto *row = world->GetLightRowTable().Row(graph::kMainLightRowIndex))
                    {
                        graph::LightRow light = *row;

                        light.direction_intensity = math::Vector4f(sun.direction.x, sun.direction.y, sun.direction.z, 4.0f);
                        light.color_angle.x = sun.color.r;
                        light.color_angle.y = sun.color.g;
                        light.color_angle.z = sun.color.b;

                        world->GetLightRowTable().SetRow(graph::kMainLightRowIndex, light);
                    }
                }
            }
        }

        LogTextureInfo("offscreen_rt_color0_init", offscreen->GetColorTexture(0));
        LogTextureInfo("offscreen_rt_depth_init",  offscreen->GetDepthTexture());
        render_context = owner->GetRenderContext();
        GLogInfo("[RTTColorDepth][OffscreenPass::Init] success world=%p rt=%p",
                 (void *)offscreen->GetWorld(), (void *)offscreen->GetRenderTarget());
        return true;
    }

    bool BuildSphere(WorkObject *owner)
    {
        GLogInfo("[RTTColorDepth][OffscreenPass::BuildSphere] begin owner=%p world=%p rt=%p",
                 (void *)owner,
                 (void *)(offscreen ? offscreen->GetWorld() : nullptr),
                 (void *)(offscreen ? offscreen->GetRenderTarget() : nullptr));
        if (!owner || !offscreen || !offscreen->IsValid())
            return LogStageFail("OffscreenPass::BuildSphere", "owner/offscreen world missing");

        GraphicsContext *gc = owner->GetGraphicsContext();
        if (!gc)
            return LogStageFail("OffscreenPass::BuildSphere", "graphics context is null");

        auto *device = gc->GetDevice();
        if (!device)
            return LogStageFail("OffscreenPass::BuildSphere", "required device missing");

        // 离屏球用 Lit 材质：消费天光/太阳方向，让 RT 纹理内容有光照（材质数值在数据块文件里，
        // 三张砖墙贴图在 RTTColorDepth.OffscreenSphere 配方文件里声明）

        if (!InitMaterialDataSSBO(owner))
            return LogStageFail("OffscreenPass::BuildSphere", "InitMaterialDataSSBO failed");

        const GeometryVertexFormat geometry_vertex_format = CreateStandardGeometryVertexFormat();
        geometry = owner->GetGeometry("geo:builtin:sphere_64", &geometry_vertex_format);
        if (!geometry)
            return LogStageFail("OffscreenPass::BuildSphere", "CreateSphere geometry failed");

        const mtl::MaterialRecipe *loaded_recipe =
            owner->GetRecipe("rcp:pack(ShaderLibrary):asset/recipe/Examples/Offscreen/RTTColorDepth.OffscreenSphere");
        if (!loaded_recipe)
            return LogStageFail("OffscreenPass::BuildSphere", "material recipe not found");
        sphere_recipe = *loaded_recipe;
        if (!(sphere_recipe.material_ssbo_binding = material_data_ssbo_accessor).IsValid())
            return LogStageFail("OffscreenPass::BuildSphere", "register material SSBO binding failed");

        // 纹理是材质原型固有属性 —— 已在 RTTColorDepth.OffscreenSphere 配方文件里声明。
        // 采样器由 Bindless 预设统一管理，材质侧不再持有 Sampler。

        sphere_asset = PrimitiveAsset(geometry, &sphere_recipe, PrimitiveType::Triangles);
        if (!sphere_asset.IsValid())
            return LogStageFail("OffscreenPass::BuildSphere", "create offscreen primitive asset failed");

        auto *world = offscreen->GetWorld();
        sphere_entity = world->CreateEntity<Entity>("OffscreenSphere");
        auto transform = world->GetTransform(world->CreateTransform(sphere_entity->GetEntityID(), Mobility::Static));
        auto prim_comp = sphere_entity->GetContextAs<hgl::ecs::ECSContext>()->GetOrCreateGeometryData(sphere_entity);
        hgl::ecs::MaterialData *material_data_comp = sphere_entity->GetContextAs<hgl::ecs::ECSContext>()->GetOrCreateMaterialData(sphere_entity);

        transform.SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        transform.SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform.SetLocalScale(glm::vec3(kSphereRadius, kSphereRadius, kSphereRadius));
        transform.SetMobility(Mobility::Static);

        prim_comp->GetOwner()->GetContextAs<hgl::ecs::ECSContext>()->GetOrCreateGeometryData(prim_comp->GetOwnerID())->SetPrimitiveAsset(&sphere_asset);
        // 纹理已由配方文件声明，实体端不再重复绑定
        hgl::ecs::MaterialData::MaterialDataAuthoringResource sphere_struct{};
        sphere_struct = material_data_ssbo_accessor;
        material_data_comp->SetDataResource(sphere_struct);
        // [A5a] 可见性真值已收敛到实体级（默认即可见）：原组件级 SetVisible(true) 等义调用已删

        sphere_primitive_comp = prim_comp;

        Entity *camera_entity = world->CreateEntity<Entity>("OffscreenCamera");
        // 相机 = **世界级资源**：经世界访问器创建/取回（实体只是宿主），不直取组件
        auto *camera = world->GetOrCreateCamera(camera_entity);
        camera->control_mode = CameraControlMode::ViewModel;
        camera->target = math::Vector3f(0, 0, 0);
        camera->distance = kCameraDistance;
        camera->yaw = 45.0f;
        camera->pitch = -20.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        // 关键：把 near/far 收紧到刚好包住球体（距相机 kCameraDistance ± kSphereRadius，
        // 再留一点余量）。引擎深度是「近端=1、远端=0」的逆序 Z（实测 d≈near/z），
        // 默认的 near=0.1 会让全部深度值落到 0.01 量级，与清屏值 0（无限远）几乎
        // 分不开，深度贴图就是一片黑——这正是之前 ShadowMap 示例看不到东西的原因。
        camera->near_plane = kCameraDistance - kSphereRadius - 0.5f;
        camera->far_plane  = kCameraDistance + kSphereRadius + 0.5f;

        GLogInfo("[RTTColorDepth][OffscreenPass::BuildSphere] tightened depth range: near=%.2f far=%.2f (camera distance=%.2f)",
                 camera->near_plane, camera->far_plane, camera->distance);

        auto camera_system = offscreen->GetCameraSystem();
        camera->viewport_info = camera_system ? camera_system->GetViewportInfo() : nullptr;

        LogStage("OffscreenPass::BuildSphere", "success");
        return true;
    }

    bool RenderOffscreenFrame()
    {
        // 清屏色已声明在 OffscreenWorldDesc::clear_color（见 Init），此处无需再传。
        // 内部走 ECSContext::RenderTo()，与主窗口路径共用同一套帧驱动。
        if (!offscreen)
            return LogStageFail("OffscreenPass::RenderOffscreenFrame", "offscreen world is null");

        auto *target=dynamic_cast<graph::OffscreenRenderTarget *>(offscreen->GetRenderTarget());
        if(s_capture_on_request && getenv("RTTCD_UNSAFE_CAPTURE_TEST") && offscreen->IsContentReady()
           && target && graph::DebugCaptureQueue::HasQueued(target->GetDebugCaptureTargetID()))
        {
            ecs::RenderPassRequest request;
            request.target=target;
            request.use_scissor=true;
            request.scissor.extent=target->GetExtent();
            return offscreen->GetWorld()->RenderTo(request);
        }
        offscreen->Render();
        return true;
    }
};

/**
 * 主画面里的显示立方体：一个立方体 + 其材质/纹理接线
 */
struct DisplayCube
{
    const char *name = nullptr;
    const char *recipe_name = nullptr;  ///< 两个立方体用不同配方名，避免材质缓存互相覆盖
    float x_offset = 0.0f;              ///< 世界坐标 X 偏移，用于并排摆放颜色与深度两个立方体
    const char *tag = nullptr;          ///< 日志标签
    const char *uri = nullptr;          ///< 纹理 URI（`rt:<RT 名>:<附件名>`）

    Geometry *geometry = nullptr;

    PrimitiveAsset asset;
    graph::mtl::MaterialRecipe recipe{};

    graph::GlobalSSBOBinding accessor{};

    /// 材质授权统一走配方文件（离屏附件用 `rt:` URI），示例不持有纹理句柄 ——
    /// 就绪判定（含离屏内容是否已画出）由 ECS 侧统一负责。

    Entity *entity = nullptr;
    hgl::ecs::TransformAccessor transform;
};

class RenderToTextureColorDepthApp final: public ulre_example::ExampleAppBase
{
public:
    RenderToTextureColorDepthApp()
    {
        SetupExampleConfig({
            "RTTCD",
            "rttcddump",
            "RTTCDSELF",
            8,
            8,
            { 1280, 720, 0, 50, 0xCF4D2D34F619C18Dull }
        });
    }

private:
    OffscreenPass *offscreen = nullptr;

    ECSContext *ecs_context = nullptr;
    Entity *camera_entity = nullptr;

    /// [0] = 贴离屏颜色纹理，[1] = 贴离屏深度纹理
    DisplayCube cubes[2];
    hgl::AutoDelete<graph::DeviceBuffer> capture_buffer;
    hgl::AutoDelete<graph::DeviceBuffer> unavailable_capture_buffer;
    uint64_t capture_buffer_id=0;
    uint64_t unavailable_capture_buffer_id=0;
    uint64_t capture_buffer_frame=0;

    float cube_theta = 0.0f;

    /// 两个立方体的 X 偏移
    static constexpr float kCubeSpacing = 1.4f;

private:
    bool InitCaptureBufferFixture()
    {
        if(!getenv("RTTCD_BUFFER_TEST"))
            return true;
        auto *device=ecs_context->GetGPUDevice();
        if(!device)
            return false;
        uint32_t upload[16];
        for(uint32_t &value:upload) value=0xa5a5a5a5u;
        capture_buffer=device->CreateBuffer(
            graph::ObjectNameBuilder("CaptureFixtureSSBO"),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            sizeof(upload),sizeof(upload),upload,graph::BufferAllocPolicy::GPUOnly,graph::SharingMode::Exclusive);
        if(!capture_buffer)
            return false;
        capture_buffer_id=graph::DebugCaptureQueue::RegisterBuffer(capture_buffer,"fixture_ssbo");
        unavailable_capture_buffer=device->CreateBuffer(
            graph::ObjectNameBuilder("UnavailableFixtureSSBO"),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            sizeof(upload),sizeof(upload),nullptr,graph::BufferAllocPolicy::GPUOnly,graph::SharingMode::Exclusive);
        if(!unavailable_capture_buffer)
            return false;
        unavailable_capture_buffer_id=graph::DebugCaptureQueue::RegisterBuffer(
            unavailable_capture_buffer,"fixture_no_transfer_src");
        return capture_buffer_id && unavailable_capture_buffer_id;
    }

    void ProcessCaptureBufferFixture()
    {
        if(!capture_buffer_id || !graph::DebugCaptureQueue::HasQueued(capture_buffer_id))
            return;
        auto *device=ecs_context->GetGPUDevice();
        if(getenv("RTTCD_BUFFER_DESTROY_TEST"))
        {
            capture_buffer_id=0;
            capture_buffer=nullptr;
            return;
        }
        if(getenv("RTTCD_BUFFER_FAIL_TEST"))
        {
            graph::DebugCaptureQueue::ProcessBuffer(capture_buffer_id,capture_buffer,
                                                     0,device->GetGraphicsQueue(),false);
            return;
        }
        if(getenv("RTTCD_BUFFER_QUEUE_TEST"))
        {
            graph::DebugCaptureQueue::ProcessBuffer(capture_buffer_id,capture_buffer,
                                                     1,VK_NULL_HANDLE,true);
            return;
        }
        hgl::AutoDelete<graph::TextureCmdBuffer> cmd(
            device->CreateTextureCommandBuffer(graph::ObjectNameBuilder("CaptureFixtureWrite")));
        bool submitted=false;
        if(cmd && cmd->Begin())
        {
            ++capture_buffer_frame;
            vkCmdFillBuffer(*cmd,capture_buffer->GetBuffer(),0,VK_WHOLE_SIZE,
                            0x12340000u+static_cast<uint32_t>(capture_buffer_frame));
            if(cmd->End())
            {
                const VkCommandBuffer raw=*cmd;
                VkSubmitInfo submit{};
                submit.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;
                submit.commandBufferCount=1;
                submit.pCommandBuffers=&raw;
                submitted=vkQueueSubmit(device->GetGraphicsQueue(),1,&submit,VK_NULL_HANDLE)==VK_SUCCESS;
            }
        }
        graph::DebugCaptureQueue::ProcessBuffer(capture_buffer_id,capture_buffer,
                                                 capture_buffer_frame,device->GetGraphicsQueue(),submitted);
        if(submitted && vkQueueWaitIdle(device->GetGraphicsQueue())!=VK_SUCCESS)
            GLogError(u8"[CaptureFixture] queue wait failed");
    }

    bool SetupMainCamera()
    {
        LogStage("App::SetupMainCamera", "begin");
        if (!ecs_context || !ecs_context->EnsureCameraSystem())
            return LogStageFail("App::SetupMainCamera", "ECS context/camera system unavailable");

        camera_entity = ecs_context->CreateEntity<Entity>("MainCamera");
        // 相机 = **世界级资源**：经世界访问器创建/取回（实体只是宿主），不直取组件
        auto *camera = ecs_context->GetOrCreateCamera(camera_entity);

        camera->control_mode = CameraControlMode::ViewModel;
        camera->target = math::Vector3f(0, 0, 0);
        camera->distance = 6.5f;        // 比 RenderToTexture 稍远，让两个立方体都进画面
        camera->yaw = 45.0f;
        camera->pitch = -20.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->viewport_info = GetViewportInfo();
        LogStage("App::SetupMainCamera", "success");
        return true;
    }

    bool InitMaterialDataSSBO(DisplayCube &cube)
    {
        if (!GetMaterialDataBinding(MATERIAL_DATA_URI, MATERIAL_DATA_ROW_CUBE, cube.accessor))
            return LogStageFail("App::InitMaterialDataSSBO", "material data block binding failed");

        return true;
    }

    bool CreateOffscreenRT()
    {
        LogStage("App::CreateOffscreenRT", "begin");
        offscreen = new OffscreenPass();
        if (!offscreen)
            return LogStageFail("App::CreateOffscreenRT", "new OffscreenPass failed");

        if (!offscreen->Init(this, 512, 512))
            return LogStageFail("App::CreateOffscreenRT", "OffscreenPass::Init failed");

        if (!offscreen->BuildSphere(this))
            return LogStageFail("App::CreateOffscreenRT", "OffscreenPass::BuildSphere failed");

        // 这里**不再**同步渲染一次：离屏世界的球体贴图是异步加载的，此刻任何一次
        // 绘制都只会得到清屏色，而且会把「已产出内容」这个状态永久钉死。
        // 由 Tick() 逐帧驱动离屏世界，待其内部资源齐备、内容真正画出来后自动停止。

        LogStage("App::CreateOffscreenRT", "success");
        return true;
    }

    /// 创建一个显示立方体：贴给定的离屏纹理
    bool CreateDisplayCube(DisplayCube &cube)
    {
        GLogInfo("[RTTColorDepth][App::CreateDisplayCube] begin name=%s x=%.2f",
                 cube.name, cube.x_offset);

        auto *gc = GetGraphicsContext();
        if (!gc)
            return LogStageFail("App::CreateDisplayCube", "graphics context is null");

        auto *gm = GetManager<GeometryManager>();
        auto *device = gc->GetDevice();
        if (!gm || !device)
            return LogStageFail("App::CreateDisplayCube", "required managers/device missing");

        if (!InitMaterialDataSSBO(cube))
            return LogStageFail("App::CreateDisplayCube", "InitMaterialDataSSBO failed");

        const GeometryVertexFormat geometry_vertex_format2 = CreateStandardGeometryVertexFormat();

        cube.geometry = GetGeometry("geo:builtin:cube", &geometry_vertex_format2);
        if (!cube.geometry)
            return LogStageFail("App::CreateDisplayCube", "CreateCube geometry failed");

        gm->Add(cube.geometry);

        const mtl::MaterialRecipe *loaded_recipe = GetRecipe(cube.recipe_name);
        if (!loaded_recipe)
            return LogStageFail("App::CreateDisplayCube", "material recipe not found");
        cube.recipe = *loaded_recipe;
        if (!(cube.recipe.material_ssbo_binding = cube.accessor).IsValid())
            return LogStageFail("App::CreateDisplayCube", "register material SSBO binding failed");

        // 两个立方体唯一的区别是配方文件里 base_color 指向的离屏附件不同（颜色 / 深度）；
        // `rt:` 身份，`CanRender` 负责等离屏内容就绪。
        // base_color 乘数保持白色：纹理内容原样进入光照（材质数值在数据块文件里）。

        cube.asset = PrimitiveAsset(cube.geometry, &cube.recipe, PrimitiveType::Triangles);
        if (!cube.asset.IsValid())
            return LogStageFail("App::CreateDisplayCube", "create cube primitive asset failed");

        cube.entity = ecs_context->CreateEntity<Entity>(cube.name);
        cube.transform = ecs_context->GetTransform(ecs_context->CreateTransform(cube.entity->GetEntityID(), Mobility::Static));
        auto prim_comp = cube.entity->GetContextAs<hgl::ecs::ECSContext>()->GetOrCreateGeometryData(cube.entity);
        hgl::ecs::MaterialData *material_data_comp = cube.entity->GetContextAs<hgl::ecs::ECSContext>()->GetOrCreateMaterialData(cube.entity);

        cube.transform.SetLocalPosition(glm::vec3(cube.x_offset, 0.0f, 0.0f));
        cube.transform.SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        cube.transform.SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        cube.transform.SetMobility(Mobility::Movable);

        prim_comp->GetOwner()->GetContextAs<hgl::ecs::ECSContext>()->GetOrCreateGeometryData(prim_comp->GetOwnerID())->SetPrimitiveAsset(&cube.asset);
        // 纹理已由配方文件声明，实体端不再重复绑定
        hgl::ecs::MaterialData::MaterialDataAuthoringResource cube_struct{};
        cube_struct = cube.accessor;
        material_data_comp->SetDataResource(cube_struct);
        // [A5a] 可见性真值已收敛到实体级（默认即可见）：原组件级 SetVisible(true) 等义调用已删

        GLogInfo("[RTTColorDepth] %s uri=%s", cube.tag, cube.uri ? cube.uri : "(null)");
        LogStage("App::CreateDisplayCube", "success");
        return true;
    }

    bool CreateDisplayCubes()
    {
        LogStage("App::CreateDisplayCubes", "begin");

        if (!offscreen)
            return LogStageFail("App::CreateDisplayCubes", "offscreen pass missing");

        cubes[0].name        = "Color";
        cubes[0].recipe_name = "RTTColorDepth.DisplayColor";
        cubes[0].tag         = "onscreen_bind_color";
        cubes[0].uri         = "rt:RTTColorDepth:OffscreenRT:color0";
        cubes[0].x_offset    = -kCubeSpacing;

        cubes[1].name        = "Depth";
        cubes[1].recipe_name = "RTTColorDepth.DisplayDepth";
        // 两个立方体都必须是「原样显示」的无光领域：
        //   · 走 Lit（PBR）会把附件值当反照率再过一遍光照 —— 颜色立方体被压暗
        //    （实测 Rmax 0.66 vs 附件 0.98），深度立方体则被环境高光染成天空色，
        //     看上去和颜色立方体几乎一样（这就是「两个都像在显示 COLOR」的根因）。
        //   · 走 UnlitTexture（2D NDC）顶点通路错，物体位置/尺寸全错。
        // 深度立方体另有一层：深度采样视图已把 R 广播到 RGB，故呈灰度。
        cubes[1].tag         = "onscreen_bind_depth";
        cubes[1].uri         = "rt:RTTColorDepth:OffscreenRT:depth";
        cubes[1].x_offset    = +kCubeSpacing;

        // 只用 URI 声明"要哪个附件"（写在 RTTColorDepth.DisplayColor / DisplayDepth 配方文件里），
        // 不拿裸纹理、不再 WrapTextureAsset：句柄在 RT 创建时已由 RenderTargetManager 绑定，
        // 此刻「容器就绪、内容未就绪」，ECS 的 CanRender 会拦住两个立方体，
        // 直到离屏 pass 真正画出内容。

        for (DisplayCube &cube : cubes)
        {
            if (!CreateDisplayCube(cube))
                return false;
        }

        LogStage("App::CreateDisplayCubes", "success");
        return true;
    }

public:
    ~RenderToTextureColorDepthApp() override
    {
        if(capture_buffer_id)
            graph::DebugCaptureQueue::UnregisterTarget(capture_buffer_id);
        if(unavailable_capture_buffer_id)
            graph::DebugCaptureQueue::UnregisterTarget(unavailable_capture_buffer_id);
        capture_buffer=nullptr;
        unavailable_capture_buffer=nullptr;
        SAFE_CLEAR(offscreen)
    }

    bool OnInit() override
    {
        LogStage("App::Init", "begin");
        ecs_context = GetECSContext();
        if (!ecs_context)
            return LogStageFail("App::Init", "ECS context is null");

        ecs_context->SetResourceNamePrefix("RTTColorDepth:MainScene");

        auto light_system = ecs_context->GetSystem<LightSystem>();
        if (!light_system)
            light_system = ecs_context->RegisterSystem<LightSystem>();

        if (light_system)
        {
            // 默认 10:00 太阳仰角 60°，本例相机平视立方体侧面（与太阳点积≈0），
            // 只剩天顶环境光会显得整体偏暗。改 8:30（仰角 37.5°）让侧面吃到直射光。
            light_system->SetMainLightTime(8, 30, 0);
        }

        if (!CreateOffscreenRT())
            return LogStageFail("App::Init", "CreateOffscreenRT failed");

        if (!CreateDisplayCubes())
            return LogStageFail("App::Init", "CreateDisplayCubes failed");

        if (!SetupMainCamera())
            return LogStageFail("App::Init", "SetupMainCamera failed");

        if(!InitCaptureBufferFixture())
            return LogStageFail("App::Init", "capture buffer fixture failed");

        LogStage("App::Init", "success");
        return true;
    }

    void OnPreTick(double delta_time) override
    {
        if (!IsSelfCheckEnabled())
        {
            cube_theta += static_cast<float>(delta_time) * 0.8f;
            cube_theta = fmodf(cube_theta, 2.0f * std::numbers::pi_v<float>);

            const glm::quat rot_z = glm::angleAxis(cube_theta, glm::vec3(0.0f, 0.0f, 1.0f));
            const glm::quat rot_x = glm::angleAxis(cube_theta * 0.5f, glm::vec3(1.0f, 0.0f, 0.0f));

            for (DisplayCube &cube : cubes)
            {
                if (cube.transform.IsValid())
                    cube.transform.SetLocalRotation(rot_z * rot_x);
            }
        }
    }

    void OnTick(double delta_time) override
    {
        ProcessCaptureBufferFixture();

        // 离屏内容静态：驱动到内容就绪即可，之后停止（避免同一纹理跨 pass 每帧写/读抖动）。
        // 就绪前每帧都要驱动 —— 离屏世界内部的异步读盘由它自己的 TextureStreamingSystem
        // 推进，不调 Render() 就没有帧，资源也就永远不会到齐。
        auto *capture_target=offscreen && s_capture_on_request
            ?dynamic_cast<graph::OffscreenRenderTarget *>(offscreen->GetRenderTarget()):nullptr;
        if (offscreen && (!offscreen->IsContentReady()
            || (capture_target && graph::DebugCaptureQueue::HasQueued(capture_target->GetDebugCaptureTargetID()))))
        {
            offscreen->RenderOffscreenFrame();
        }

        // 自检与落盘都只从「离屏内容已就绪」之后开始计数 —— 否则第 8 帧可能仍落在
        // 异步读盘尚未完成的窗口里，摘要就会把"还没加载完"当成"离屏没画"。
        if (offscreen && !offscreen->IsContentReady())
        {
            example_frame_seen = 0;
            return;
        }
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<RenderToTextureColorDepthApp>(OS_TEXT("Render To Texture - Color vs Depth"), argc, argv, 1280, 720);
}
