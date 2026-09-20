#include<hgl/framework/WorkManager.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKTexture.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/module/OffscreenWorld.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/MaterialSSBOBufferRegistry.h>
#include<hgl/graph/ssbo/MaterialDataRows.h>

#include<hgl/graph/module/EnvironmentManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/color/Color.h>
#include<hgl/log/Log.h>
#include<hgl/mtl/MaterialRecipe.h>

#include<hgl/graph/ssbo/LitMaterialData.h>

#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/RenderTargetSystem.h>
#include<hgl/ecs/systems/render/RenderSystemCore.h>
#include<hgl/ecs/systems/render/EnvironmentSystem.h>
#include<hgl/ecs/systems/render/RenderSceneUBOSystem.h>
#include<hgl/ecs/systems/tick/InputSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<glm/gtx/quaternion.hpp>

#include<vector>
#include<string>
#include<memory>
#include<cmath>
#include<numbers>

/**
 * ShadowMap —— 以 BasicLitMeshes 为蓝本的"阴影接收"场景 + depth-only shadow map
 *
 * ## 场景蓝本
 *
 * 场景整体取自 example/Basic/BasicLitMeshes.cpp：
 *   - 同一套几何体清单（PlaneSquare + 13 个标准网格）
 *   - 同一套 Brickwall 材质（base_color / normal / roughness + Lit 配方）
 *   - 同一套顶点格式（TexCoord=RG16F、Normal=RG8 octahedral，不存切线）
 *   - 同样的环状摆放：13 个网格均布在半径 6.5 的圆环上（Z 轴向上，XY 为地面）
 *
 * ## 与蓝本的差异（本用例的关键改动）
 *
 *   1. **PlaneSquare 放大 kReceiverPlaneScale 倍**：蓝本里它是原点处 1×1 的地砖，
 *      本用例把它放大成一整块地面 —— 它就是阴影的**接收面**（receiver）。
 *   2. 环上网格按各自 AABB 的最低点抬升到地面之上。蓝本里所有网格中心都在 z=0，
 *      地砖很小时看不出来；地面放大后不抬升会有一半埋进地里。
 *   3. 增加一个光源视角的 depth-only 离屏 Pass（shadow map）。
 *   4. **地面改用 ShadowReceiver 材质**（ShaderLibrary/material/shadow_receiver.material.toml）：
 *      它把 shadow map 采样成遮挡遮罩去乘 albedo，于是环上所有网格的影子
 *      都投在这块地面上。
 *
 * ## 阴影接收是怎么做的（以及为什么能这么做）
 *
 * 渲染链路：
 *
 *   [ShadowDepthPass]  光源相机 + depth-only 离屏世界 → 1024×1024 深度图
 *          │                                       （只画环上网格，**不画地面**）
 *          └────────────► Texture2D*（SHADER_READ_ONLY_OPTIMAL）
 *                              │
 *   [主世界] 地面实体 ── SetMaterialTextureResource("shadow_map", ...) ──┘
 *            ShadowReceiver 材质源： uv0 → 光源空间 uv → 采样 → 遮挡遮罩 → 乘 albedo
 *
 * 为了只动"ShaderLibrary + 示例"两层（不改引擎 ABI），这里做了一个关键约束：
 *
 *   **光源相机摆成正俯视，且 fov 取 2*atan(地面半宽 / 相机高度)。**
 *
 * 于是光锥在地面处的截面恰好等于地面，地面在 shadow map 里铺满 [0,1]²；
 * 又因为地面垂直于光轴、到光源的视线方向距离处处相等，"世界 XY → 光源空间 uv"
 * 退化成纯线性映射，而 PlaneSquare 的 uv0 本身就是 0.5 + 世界XY/20。
 * 两者只差一个 Y 镜像（引擎投影矩阵 m[1][1] = -f，见 ReversedZProj.cpp），
 * 所以在 shader 里把 v 翻回来即可。
 *
 * 代价（也就是"只在示例层做"的边界）：
 *   地面必须是平的、光源必须接近正上方。通用做法需要引擎侧补齐下面四项。
 *
 * ## TODO：引擎侧要补齐什么（截至本次改动均未实现）
 *
 *   a. **光照空间矩阵**：Scene UBO 只有 Camera/Sky/Viewport/ColorPalette/
 *      GlobalAddresses 五个 binding，没有 light VP。需新增 Shadow UBO
 *      （SceneBinding 枚举 + C++ 绑定 + ShaderLibrary/ubo/*.glsl）；
 *   b. **shadow provider**：ShaderLibrary/shadow/ 下只有 identity.glsl，
 *      GetShadowFactor() 恒返回 1.0。需实现按光照空间投影采样 + 深度比较（PCF）；
 *   c. **模板接线**：fragment/forward_lit.glsl.tmpl 的主流程当前**根本不调用**
 *      GetShadowFactor，LightingInput 里也没有 shadow 字段；
 *   d. **材质**：lit.material.toml 需增加 shadow_map 纹理槽。
 *
 * ## 验证目标
 *
 *   1. RenderTargetDesc::OffscreenDepthOnly 能建出零颜色附件的 RT
 *      —— color_count == 0、has_depth == true、深度纹理非空
 *   2. depth-only 目标的材质管线能通过 PipelineResolver 的校验并成功创建
 *   3. 渲染结束后深度纹理处于可采样布局（SHADER_READ_ONLY_OPTIMAL）
 *   4. 该深度纹理可以绑定到主世界材质并采样（ShadowReceiver 材质槽 shadow_map）
 *   5. 地面上能看到环上网格投下的影子
 */

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{
    // ── 场景参数（与 BasicLitMeshes 对齐） ──────────────────────────

    /// 环上网格的分布半径（蓝本原值）
    static constexpr float  kMeshRingRadius      = 6.5f;

    /// PlaneSquare 的放大倍数：蓝本是 1×1 的地砖，这里放大成整块地面
    static constexpr float  kReceiverPlaneScale  = 20.0f;

    /// 地面半宽（= 放大后 PlaneSquare 的半径，uv0 的 0.5 → 10 个世界单位）
    static constexpr float  kReceiverHalfExtent  = kReceiverPlaneScale * 0.5f;

    // ── 光源视角相机（shadow map 拍摄） ────────────────────────────

    /// 光源相机到原点的距离（= 光源高度）
    static constexpr float  kLightDistance = 36.0f;

    /// 正俯视的朝向。
    ///
    /// pitch **不能取 -90**：CameraSystem::ComputeRightUp 用
    /// normalize(cross(forward, world_up))，而 forward 与 world_up 平行时叉积为 0，
    /// 一拍即 NaN（该帧相机矩阵全废）。取 -89.9（偏 0.1°）既能保证
    /// cross 的方向稳定（right = +X），投影误差也只有 0.05%，远小于 PCF 半径。
    /// yaw=90 是刻意选的：此时的 right/up 恰好是 +X/+Y，屏幕轴与世界轴对齐。
    static constexpr float  kLightYaw   = 90.0f;
    static constexpr float  kLightPitch = -89.9f;

    /// 光源相机 fov：让光锥在地面处的截面**恰好等于地面本身**
    ///     tan(fov/2) = 地面半宽 / 相机高度 = 10 / 36
    ///     fov        = 2 * atan(10/36) = 31.0482°
    /// 于是地面在 shadow map 里正好铺满 [0,1]²，uv0 可直接当光源空间 uv 用。
    static constexpr float  kLightFov = 31.0482f;

    /// 近平面。reverse-Z + 无限远投影下深度 = near / lin（lin 为沿光轴的视线距离），
    /// near 必须小于场景里离光源最近的几何体：最高的是 Torus（抬升后顶点 z≈4.2），
    /// 即 lin_min ≈ 31.8，取 28 留足余量（否则会把网格裁掉一块）。
    static constexpr float  kLightNear = 28.0f;

    /// 远平面。reversed-Z 走 MakeInfiniteReversedZProj，far 不参与运算，
    /// 只写在相机上作为语义说明。
    static constexpr float  kLightFar = 64.0f;

    // ── 主相机 ─────────────────────────────────────────────────────

    /// 距离比蓝本的 14 大：地面放大到 20×20，退远一点才装得下整个环 + 地面边缘。
    static constexpr float  kMainDistance = 22.0f;
    static constexpr float  kMainYaw      = 45.0f;

    /// 与蓝本一致的 -20°。俯角太小会让地面几乎退化成一条线，太大又会让
    /// "物体正下方的影子"被物体自身挡住 —— 20° 是两者之间可读性最好的一档。
    static constexpr float  kMainPitch    = -20.0f;

    // ── shadow map ─────────────────────────────────────────────────

    static constexpr uint32_t kShadowMapSize = 1024;

    GeometryVertexFormat CreateStandardGeometryVertexFormat()
    {
        // 与 BasicLitMeshes 一致：UV0 用 RG16F（half×2，4B/顶点）、法线用 RG8
        // octahedral（2B/顶点），完全不存切线。
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V3F},
            {VertexSemantic::TexCoord, VF_V2HF},
            {VertexSemantic::Normal,   VF_V2UN8},
        };
        return gvf;
    }

    bool LogStageFail(const char *stage, const char *reason)
    {
        GLogError("[ShadowMap][%s] %s", stage, reason);
        return false;
    }

    void LogStage(const char *stage, const char *message)
    {
        GLogInfo("[ShadowMap][%s] %s", stage, message);
    }

    /// 打印深度目标的关键状态，作为"链路是否打通"的客观证据
    /// @param require_samplable 为真时要求深度已处于可采样布局（渲染后才成立；
    ///        刚创建、尚未渲染的目标停留在 attachment 布局是正常的）
    void ReportDepthTarget(const char *stage, IRenderTarget *rt, Texture2D *depth_tex, bool require_samplable)
    {
        if (!rt)
        {
            GLogError("[ShadowMap][%s] render target is null", stage);
            return;
        }

        GLogInfo("[ShadowMap][%s] color_count=%u has_depth=%d depth_tex=%p",
                 stage,
                 rt->GetColorCount(),
                 rt->hasDepth() ? 1 : 0,
                 (void *)depth_tex);

        if (!depth_tex)
        {
            GLogError("[ShadowMap][%s] depth texture is null", stage);
            return;
        }

        GLogInfo("[ShadowMap][%s] depth_image=%p view=%p format=%d layout=%u extent=%ux%u",
                 stage,
                 (void *)depth_tex->GetImage(),
                 (void *)depth_tex->GetVulkanImageView(),
                 (int)depth_tex->GetFormat(),
                 (uint32_t)depth_tex->GetImageLayout(),
                 depth_tex->GetWidth(),
                 depth_tex->GetHeight());

        // 可采样判据：渲染结束后深度必须离开 attachment 布局。
        // 5 == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        // 3 == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL（未转换，无法采样）
        const bool samplable = ((uint32_t)depth_tex->GetImageLayout() == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        if (!require_samplable)
        {
            GLogInfo("[ShadowMap][%s] depth layout before render (samplable=%d)", stage, samplable ? 1 : 0);
            return;
        }

        if (samplable)
            LogStage(stage, "depth layout is SHADER_READ_ONLY_OPTIMAL (samplable)");
        else
            GLogError("[ShadowMap][%s] depth layout is NOT samplable (layout=%u)",
                      stage, (uint32_t)depth_tex->GetImageLayout());
    }

    /// 与 BasicLitMeshes 相同的环状摆放：绕 Z 轴均布在半径 kMeshRingRadius 的圆环上
    glm::vec3 RingPosition(const size_t index, const size_t count)
    {
        const float angle = glm::radians(360.0f * static_cast<float>(index) / static_cast<float>(count));
        const glm::quat rotation = glm::angleAxis(angle, glm::vec3(0.0f, 0.0f, 1.0f));

        return glm::rotate(rotation, glm::vec3(kMeshRingRadius, 0.0f, 0.0f));
    }

    glm::quat RingRotation(const size_t index, const size_t count)
    {
        const float angle = glm::radians(360.0f * static_cast<float>(index) / static_cast<float>(count));
        return glm::angleAxis(angle, glm::vec3(0.0f, 0.0f, 1.0f));
    }

    /// 求"把几何体落到 z=0 地面上"所需的抬升量。
    ///
    /// inline_geometry 的各创建函数都会用 CreateWithAABB() 写入各自的包围盒，
    /// 因此直接取 AABB 的 min.z 取反即可；包围盒为空时返回 0（保持原位）。
    float GroundLift(const Geometry *geom)
    {
        if (!geom)
            return 0.0f;

        const math::BoundingVolumes &bv = geom->GetBoundingVolumes();
        if (bv.aabb.IsEmpty())
            return 0.0f;

        const float min_z = bv.aabb.GetMin().z;
        return (min_z < 0.0f) ? -min_z : 0.0f;
    }
}

/// 场景资产：接收面 + 环上网格。离屏世界与主世界**共用同一批 Geometry**，
/// 只是各自建一份 PrimitiveAsset（asset 本身不持有 Geometry 所有权）。
class ShadowScene
{
public:

    Geometry *                  receiver_plane = nullptr;   ///< 放大的 PlaneSquare（阴影接收面）
    std::vector<Geometry *>     meshes;                     ///< 环上的标准网格
    std::vector<float>          mesh_lift;                  ///< 各网格落到地面所需的抬升量

    /// 接收面在**离屏世界**用：普通 Lit 材质（阴影 Pass 里根本不画它，
    /// 这里只是保持资源完整）
    PrimitiveAsset              plane_asset{};

    /// 接收面在**主世界**用：ShadowReceiver 材质（会采样 shadow map）
    PrimitiveAsset              receiver_asset{};

    std::vector<PrimitiveAsset> mesh_assets;

    bool IsValid()const
    {
        return receiver_plane
            && plane_asset.IsValid()
            && receiver_asset.IsValid()
            && !meshes.empty()
            && meshes.size() == mesh_assets.size()
            && meshes.size() == mesh_lift.size();
    }
};

/// 光源视角的 depth-only Pass：把整块场景（只有网格，没有地面）渲染进 shadow map
class ShadowDepthPass
{
private:

    std::unique_ptr<graph::OffscreenWorld> offscreen;

public:

    ~ShadowDepthPass() = default;

    Texture2D *GetDepthTexture() const
    {
        return offscreen ? offscreen->GetDepthTexture() : nullptr;
    }

    IRenderTarget *GetRenderTarget() const
    {
        return offscreen ? offscreen->GetRenderTarget() : nullptr;
    }

    ECSContext *GetWorld() const
    {
        return offscreen ? offscreen->GetWorld() : nullptr;
    }

    std::shared_ptr<ecs::CameraSystem> GetCameraSystem() const
    {
        return offscreen ? offscreen->GetCameraSystem() : nullptr;
    }

    bool Init(WorkObject *owner, const uint32_t size)
    {
        LogStage("ShadowDepthPass::Init", "begin");

        if (!owner)
            return LogStageFail("ShadowDepthPass::Init", "owner is null");

        // depth-only 离屏世界：零颜色附件，深度渲染后可采样
        graph::OffscreenWorldDesc desc;
        desc.width           = size;
        desc.height          = size;
        desc.name            = "ShadowMap_DepthWorld";
        desc.resource_prefix = "ShadowMap:ShadowMap";
        desc.depth_only      = true;
        desc.depth_format    = PF_D32F;     // 纯深度格式：顺带验证 stencil 附件声明的处理

        offscreen = graph::OffscreenWorld::Create(owner->GetGraphicsContext(),
                                                  owner->GetECSContext(),
                                                  desc);
        if (!offscreen || !offscreen->IsValid())
            return LogStageFail("ShadowDepthPass::Init", "OffscreenWorld::Create failed");

        if (auto *rt = GetRenderTarget())
        {
            if (rt->GetColorCount() != 0)
            {
                GLogError("[ShadowMap][ShadowDepthPass::Init] expected depth-only target but color_count=%u",
                          rt->GetColorCount());
                return false;
            }

            if (!rt->hasDepth())
                return LogStageFail("ShadowDepthPass::Init", "depth-only target has no depth attachment");
        }

        ReportDepthTarget("ShadowDepthPass::Init", GetRenderTarget(), GetDepthTexture(), false);

        LogStage("ShadowDepthPass::Init", "success");
        return true;
    }

    /// 以光源视角渲染一帧，把场景写进 shadow map
    bool Render()
    {
        LogStage("ShadowDepthPass::Render", "begin");

        if (!offscreen)
            return LogStageFail("ShadowDepthPass::Render", "offscreen world is null");

        offscreen->Render();

        ReportDepthTarget("ShadowDepthPass::Render", GetRenderTarget(), GetDepthTexture(), true);

        LogStage("ShadowDepthPass::Render", "success");
        return true;
    }
};

class ShadowMapApp final : public WorkObject
{
private:

    ECSContext *ecs_context = nullptr;

    Entity *main_camera_entity = nullptr;

    VertexDataManager *scene_vdm = nullptr;

    ShadowScene scene;

    graph::mtl::MaterialRecipe scene_recipe{};
    graph::mtl::MaterialRecipe receiver_recipe{};

    using MaterialDataAccessor = graph::MaterialSSBODataAccessor;
    using MaterialBinding      = hgl::ecs::PrimitiveComponent::MaterialDataAuthoringResource;

    MaterialDataAccessor material_data_ssbo_accessor{};
    graph::ssbo::PBRSurfaceRow scene_material_data{};
    MaterialBinding scene_material_binding{};

    Texture2D *base_texture      = nullptr;
    Texture2D *normal_texture    = nullptr;
    Texture2D *roughness_texture = nullptr;
    Sampler   *scene_sampler     = nullptr;

    /// 材质纹理绑定的 sampler 只是"已授权"的凭证（见 PrimitiveComponent::
    /// AppendTextureBinding —— sampler 不写进 recipe），真正生效的采样器是
    /// shader 里的编译期预设宏（shadow map 走 ShadowMapSampler = Nearest）。
    /// 这里复用一个 sampler 即可。
    Sampler   *shadow_sampler = nullptr;

    ShadowDepthPass *depth_pass = nullptr;

private:

    // ── 初始化 ────────────────────────────────────────────────────────

    bool InitVDM()
    {
        auto *buffer_manager = GetManager<BufferManager>();
        if (!buffer_manager)
            return LogStageFail("ShadowMapApp::InitVDM", "buffer manager is null");

        scene_vdm = new VertexDataManager(buffer_manager, CreateStandardGeometryVertexFormat());
        if (!scene_vdm)
            return LogStageFail("ShadowMapApp::InitVDM", "create VertexDataManager failed");

        if (!scene_vdm->Init(HGL_SIZE_1MB, HGL_SIZE_1MB, IndexType::U32))
            return LogStageFail("ShadowMapApp::InitVDM", "VertexDataManager::Init failed");

        return true;
    }

    bool InitMaterialDataSSBO()
    {
        auto *domain_manager = GetManager<MaterialSSBOBufferRegistry>();
        if (!domain_manager)
            return LogStageFail("ShadowMapApp::InitMaterialDataSSBO", "material ssbo registry is null");

        // 与 BasicLitMeshes 相同的 PBR 参数
        scene_material_data.base_color   = Color4f(1.0f);
        scene_material_data.metallic     = 0.08f;
        scene_material_data.roughness    = 0.92f;
        scene_material_data.normal_scale = 0.35f;

        material_data_ssbo_accessor = domain_manager->GetMaterialDataAccessor<graph::ssbo::PBRSurfaceRow>();
        if (!material_data_ssbo_accessor)
            return LogStageFail("ShadowMapApp::InitMaterialDataSSBO", "create accessor failed");

        if (!material_data_ssbo_accessor.Write(scene_material_data))
            return LogStageFail("ShadowMapApp::InitMaterialDataSSBO", "write material data failed");

        scene_material_binding = material_data_ssbo_accessor.GetMaterialSSBOBinding();
        if (!scene_material_binding.IsValid())
            return LogStageFail("ShadowMapApp::InitMaterialDataSSBO", "material SSBO binding invalid");

        return true;
    }

    bool InitMaterial()
    {
        auto *texture_manager = GetManager<TextureManager>();
        auto *sampler_manager = GetManager<SamplerManager>();

        if (!texture_manager || !sampler_manager)
            return LogStageFail("ShadowMapApp::InitMaterial", "texture/sampler manager missing");

        // ── 场景材质（环上网格用，与 BasicLitMeshes 同一套砖墙贴图） ──
        scene_recipe.recipe_name = "ShadowMap.Scene";
        scene_recipe.mtl_def_id  = "Lit";
        scene_recipe.render_state_overrides.pipeline_config = mtl::MakeSolid3DConfig();
        if (!(scene_recipe.material_ssbo_binding = scene_material_binding).IsValid())
            return LogStageFail("ShadowMapApp::InitMaterial", "scene recipe material SSBO binding invalid");

        // ── 接收面材质（地面用，会采样 shadow map） ──
        // 定义在 ShaderLibrary/material/shadow_receiver.material.toml：
        // 与 Lit 同构，只是 material_source_module 换成了 shadow_receiver_source.glsl
        receiver_recipe.recipe_name = "ShadowMap.Receiver";
        receiver_recipe.mtl_def_id  = "ShadowReceiver";
        receiver_recipe.render_state_overrides.pipeline_config = mtl::MakeSolid3DConfig();
        if (!(receiver_recipe.material_ssbo_binding = scene_material_binding).IsValid())
            return LogStageFail("ShadowMapApp::InitMaterial", "receiver recipe material SSBO binding invalid");

        base_texture = texture_manager->LoadTexture2D(OS_TEXT("res/image/Brickwall/Albedo.Tex2D"), true);
        if (!base_texture)
            return LogStageFail("ShadowMapApp::InitMaterial", "load Brickwall Albedo failed");

        normal_texture = texture_manager->LoadTexture2D(OS_TEXT("res/image/Brickwall/Normal.Tex2D"), true);
        if (!normal_texture)
            return LogStageFail("ShadowMapApp::InitMaterial", "load Brickwall Normal failed");

        roughness_texture = texture_manager->LoadTexture2D(OS_TEXT("res/image/Brickwall/Roughness.Tex2D"), true);
        if (!roughness_texture)
            return LogStageFail("ShadowMapApp::InitMaterial", "load Brickwall Roughness failed");

        scene_sampler = sampler_manager->CreateSampler();
        if (!scene_sampler)
            return LogStageFail("ShadowMapApp::InitMaterial", "create scene sampler failed");

        shadow_sampler = sampler_manager->CreateSampler();
        if (!shadow_sampler)
            return LogStageFail("ShadowMapApp::InitMaterial", "create shadow sampler failed");

        return true;
    }

    /// 建出场景几何体：接收面 PlaneSquare + 蓝本里的 13 个标准网格
    bool CreateSceneGeometry()
    {
        LogStage("ShadowMapApp::CreateSceneGeometry", "begin");

        auto *gm = GetManager<GeometryManager>();
        if (!gm || !scene_vdm)
            return LogStageFail("ShadowMapApp::CreateSceneGeometry", "geometry manager / VDM missing");

        using namespace inline_geometry;

        auto create_geometry = [this](auto &&creator) -> Geometry *
        {
            auto pc = std::make_unique<GeometryCreater>(scene_vdm);
            if (!pc)
                return nullptr;

            return creator(pc.get());
        };

        // ── 接收面：放大的 PlaneSquare（蓝本里它是原点处 1×1 的地砖） ──
        {
            auto *geom = create_geometry([](GeometryCreater *pc)
            {
                return CreatePlaneSqaure(pc);
            });
            if (!geom)
                return LogStageFail("ShadowMapApp::CreateSceneGeometry", "CreatePlaneSqaure failed");

            gm->Add(geom);
            scene.receiver_plane = geom;

            GLogInfo("[ShadowMap][ShadowMapApp::CreateSceneGeometry] receiver plane = PlaneSquare x%.1f (蓝本为 1x1) uv0.y 与世界 +Y 同向",
                     kReceiverPlaneScale);
        }

        auto add_mesh = [&](const char *name, Geometry *geom) -> bool
        {
            if (!geom)
            {
                GLogError("[ShadowMap][ShadowMapApp::CreateSceneGeometry] create mesh failed: %s", name);
                return false;
            }

            gm->Add(geom);

            const float lift = GroundLift(geom);

            scene.meshes.push_back(geom);
            scene.mesh_lift.push_back(lift);

            GLogInfo("[ShadowMap][ShadowMapApp::CreateSceneGeometry] mesh[%zu] %s ground_lift=%.3f",
                     scene.meshes.size() - 1, name, lift);
            return true;
        };

        // ── 以下清单与 BasicLitMeshes 完全一致（顺序也保持一致） ──
        if (!add_mesh("Sphere", create_geometry([](GeometryCreater *pc)
            { return CreateSphere(pc, 64); })))
            return false;

        if (!add_mesh("Dome", create_geometry([](GeometryCreater *pc)
            { return CreateDome(pc, 64); })))
            return false;

        if (!add_mesh("Cone", create_geometry([](GeometryCreater *pc)
            {
                ConeCreateInfo cci;
                cci.radius = 1;
                cci.halfExtend = 1;
                cci.numberSlices = 64;
                cci.numberStacks = 4;
                return CreateCone(pc, &cci);
            })))
            return false;

        if (!add_mesh("Cylinder", create_geometry([](GeometryCreater *pc)
            {
                CylinderCreateInfo cci;
                cci.halfExtend = 1.25f;
                cci.numberSlices = 16;
                cci.radius = 1.25f;
                return CreateCylinder(pc, &cci);
            })))
            return false;

        if (!add_mesh("Torus", create_geometry([](GeometryCreater *pc)
            {
                TorusCreateInfo tci;
                tci.innerRadius = 1.9f;
                tci.outerRadius = 2.1f;
                tci.numberSlices = 128;
                tci.numberStacks = 16;
                return CreateTorus(pc, &tci);
            })))
            return false;

        if (!add_mesh("HollowCylinder", create_geometry([](GeometryCreater *pc)
            {
                HollowCylinderCreateInfo hcci;
                hcci.halfExtend = 1.25f;
                hcci.innerRadius = 0.8f;
                hcci.outerRadius = 1.25f;
                hcci.numberSlices = 64;
                return CreateHollowCylinder(pc, &hcci);
            })))
            return false;

        if (!add_mesh("HexSphere", create_geometry([](GeometryCreater *pc)
            {
                HexSphereCreateInfo hsci;
                hsci.subdivisions = 3;
                return CreateHexSphere(pc, &hsci);
            })))
            return false;

        if (!add_mesh("Capsule", create_geometry([](GeometryCreater *pc)
            {
                CapsuleCreateInfo cci;
                return CreateCapsule(pc, &cci);
            })))
            return false;

        if (!add_mesh("TaperedCapsule", create_geometry([](GeometryCreater *pc)
            {
                TaperedCapsuleCreateInfo tcci;
                tcci.topRadius = 0.1f;
                return CreateTaperedCapsule(pc, &tcci);
            })))
            return false;

        if (!add_mesh("Cube", create_geometry([](GeometryCreater *pc)
            {
                CubeCreateInfo cci;
                cci.segments_x = 1;
                cci.segments_y = 1;
                cci.segments_z = 1;
                cci.ntb = NTBType::Normal;
                return CreateCube(pc, &cci);
            })))
            return false;

        if (!add_mesh("Frustum", create_geometry([](GeometryCreater *pc)
            {
                FrustumCreateInfo fci;
                fci.bottom_radius = 1.0f;
                fci.top_radius = 0.5f;
                fci.height = 2.0f;
                fci.numberSlices = 32;
                return CreateFrustum(pc, &fci);
            })))
            return false;

        if (!add_mesh("Arrow", create_geometry([](GeometryCreater *pc)
            {
                ArrowCreateInfo aci;
                aci.shaft_radius = 0.1f;
                aci.shaft_length = 2.0f;
                aci.head_radius = 0.3f;
                aci.head_length = 0.5f;
                aci.numberSlices = 16;
                aci.cross_section = ArrowCrossSection::Circular;
                return CreateArrow(pc, &aci);
            })))
            return false;

        if (!add_mesh("PipeElbow", create_geometry([](GeometryCreater *pc)
            {
                PipeElbowCreateInfo peci;
                peci.inner_radius = 0.3f;
                peci.outer_radius = 0.5f;
                peci.bend_angle = 90.0f;
                peci.bend_radius = 1.0f;
                peci.pipe_segments = 16;
                peci.bend_segments = 16;
                return CreatePipeElbow(pc, &peci);
            })))
            return false;

        // ── 组装资产（离屏 / 主世界各一份，引用同一批 Geometry） ──
        // 接收面要两份：离屏世界用 Lit（虽然阴影 Pass 不画地面，保持资源完整），
        // 主世界用 ShadowReceiver（采样 shadow map）
        scene.plane_asset = PrimitiveAsset(scene.receiver_plane, &scene_recipe, PrimitiveType::Triangles);
        if (!scene.plane_asset.IsValid())
            return LogStageFail("ShadowMapApp::CreateSceneGeometry", "create receiver plane asset failed");

        scene.receiver_asset = PrimitiveAsset(scene.receiver_plane, &receiver_recipe, PrimitiveType::Triangles);
        if (!scene.receiver_asset.IsValid())
            return LogStageFail("ShadowMapApp::CreateSceneGeometry", "create shadow-receiver plane asset failed");

        scene.mesh_assets.reserve(scene.meshes.size());
        for (auto *geom : scene.meshes)
        {
            PrimitiveAsset asset(geom, &scene_recipe, PrimitiveType::Triangles);
            if (!asset.IsValid())
                return LogStageFail("ShadowMapApp::CreateSceneGeometry", "create mesh asset failed");

            scene.mesh_assets.push_back(asset);
        }

        if (!scene.IsValid())
            return LogStageFail("ShadowMapApp::CreateSceneGeometry", "scene assets invalid");

        LogStage("ShadowMapApp::CreateSceneGeometry", "success");
        return true;
    }

    /// 给环上网格挂场景材质（砖墙 base_color/normal/roughness + PBR 参数）。
    /// 这些网格只负责**投射**阴影，材质本身与蓝本 BasicLitMeshes 完全一致。
    void ApplyMeshMaterial(PrimitiveComponent *prim)
    {
        if (!prim)
            return;

        prim->SetMaterialTextureResource("base_color", base_texture, scene_sampler);
        prim->SetMaterialTextureResource("normal", normal_texture, scene_sampler);
        prim->SetMaterialTextureResource("roughness", roughness_texture, scene_sampler);
        prim->SetMaterialDataResource(scene_material_binding);
        prim->SetVisible(true);
    }

    /// 给接收面挂 ShadowReceiver 材质
    ///
    /// @param shadow_map 光源视角的深度图；为 nullptr 时只挂 albedo（材质里的
    ///        shadow_map 槽保持未绑定 → EvalShadowReceiverMask 返回 1.0，即全亮）
    void ApplyReceiverMaterial(PrimitiveComponent *prim, Texture2D *shadow_map)
    {
        if (!prim)
            return;

        // 接收面是 1×1 UV 铺满 20×20 的大平面，normal/roughness 会被放大 20 倍
        // 糊成一片，反而把地面弄脏；所以这里只保留 base_color。
        prim->SetMaterialTextureResource("base_color", base_texture, scene_sampler);

        if (shadow_map)
        {
            if (!prim->SetMaterialTextureResource("shadow_map", shadow_map, shadow_sampler))
                GLogError("[ShadowMap][ApplyReceiverMaterial] bind shadow_map failed");
        }

        prim->SetMaterialDataResource(scene_material_binding);
        prim->SetVisible(true);
    }

    /// 把场景铺进指定世界。
    /// 离屏世界与主世界用的是同一批 Geometry / 同一份材质数据，只是各自建实体。
    ///
    /// @param include_receiver 是否放进接收面。
    ///        阴影 Pass **不放**：地面是接收者不是投射者，不写进深度图既能
    ///        彻底避免自遮挡（acne），也让"裸地 = 清屏值"成为干净的基准。
    bool PopulateScene(ECSContext *world, const char *stage, const bool include_receiver, Texture2D *shadow_map)
    {
        if (!world)
            return LogStageFail(stage, "world is null");

        if (!scene.IsValid())
            return LogStageFail(stage, "scene assets are not ready");

        // ── 接收面：PlaneSquare 在 XY 平面，放大后就是整块地面 ──
        if (include_receiver)
        {
            auto *entity = world->CreateEntity<Entity>("ShadowReceiverPlane");
            auto transform = entity->AddComponent<TransformComponent>(Mobility::Static);
            auto prim_comp = entity->AddComponent<PrimitiveComponent>();

            transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
            transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            transform->SetLocalScale(glm::vec3(kReceiverPlaneScale, kReceiverPlaneScale, 1.0f));
            transform->SetMovable(false);

            prim_comp->SetPrimitiveAsset(&scene.receiver_asset);
            ApplyReceiverMaterial(prim_comp.get(), shadow_map);
        }

        // ── 环上网格：与蓝本相同的排布，额外按 AABB 抬到地面之上 ──
        const size_t count = scene.meshes.size();
        for (size_t i = 0; i < count; ++i)
        {
            auto *entity = world->CreateEntity<Entity>("Mesh_" + std::to_string(i));
            auto transform = entity->AddComponent<TransformComponent>(Mobility::Static);
            auto prim_comp = entity->AddComponent<PrimitiveComponent>();

            glm::vec3 pos = RingPosition(i, count);
            pos.z += scene.mesh_lift[i];

            transform->SetLocalPosition(pos);
            transform->SetLocalRotation(RingRotation(i, count));
            transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
            transform->SetMovable(false);

            prim_comp->SetPrimitiveAsset(&scene.mesh_assets[i]);
            ApplyMeshMaterial(prim_comp.get());
        }

        GLogInfo("[ShadowMap][%s] populated %zu meshes + receiver(%d)", stage, count, include_receiver ? 1 : 0);
        return true;
    }

    /// 光源视角相机：**正俯视**整块地面。
    ///
    /// 摆法由"阴影接收方案"决定（详见文件头）：
    ///   - yaw=90 / pitch≈-90：正上方垂直向下，且 right/up 恰好是 +X/+Y，
    ///     屏幕轴与世界轴对齐；
    ///   - fov = 2*atan(地面半宽/高度)：光锥在地面处的截面恰好等于地面，
    ///     地面在 shadow map 里铺满 [0,1]²；
    ///   - near 收紧到 28（见 kLightNear 注释）。
    bool CreateLightCamera(ECSContext *world)
    {
        if (!world)
            return LogStageFail("ShadowMapApp::CreateLightCamera", "world is null");

        auto *camera_system = depth_pass ? depth_pass->GetCameraSystem().get() : nullptr;
        if (!camera_system)
            return LogStageFail("ShadowMapApp::CreateLightCamera", "camera system unavailable");

        auto *entity = world->CreateEntity<Entity>("LightCamera");
        auto camera = entity->AddComponent<CameraComponent>();

        camera->control_mode = CameraComponent::ControlMode::ViewModel;
        camera->target = math::Vector3f(0, 0, 0);
        camera->distance = kLightDistance;
        camera->yaw = kLightYaw;
        camera->pitch = kLightPitch;
        camera->fov = kLightFov;
        camera->near_plane = kLightNear;
        camera->far_plane = kLightFar;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->camera_data   = camera_system->GetCamera();
        camera->camera_info   = const_cast<CameraInfo *>(camera_system->GetCameraInfo());
        camera->viewport_info = camera_system->GetViewportInfo();

        GLogInfo("[ShadowMap][ShadowMapApp::CreateLightCamera] distance=%.2f yaw=%.1f pitch=%.1f fov=%.4f near=%.2f far=%.2f",
                 kLightDistance, kLightYaw, kLightPitch, kLightFov, camera->near_plane, camera->far_plane);

        // 这些常量必须与 ShaderLibrary/material/shadow_receiver_source.glsl 里
        // 的假设一致：地面铺满 shadow map、uv0 == 光源空间 uv（差一个 Y 镜像）。
        GLogInfo("[ShadowMap][ShadowMapApp::CreateLightCamera] 地面半宽=%.1f 期望 fov=%.4f 实际 fov=%.4f (tan(half)=%.6f 应为 %.6f)",
                 kReceiverHalfExtent, kLightFov, camera->fov,
                 tanf(camera->fov * 0.5f * 3.14159265358979f / 180.0f),
                 kReceiverHalfExtent / kLightDistance);

        return true;
    }

    bool SetupMainCamera()
    {
        if (!ecs_context || !ecs_context->EnsureCameraSystem())
            return LogStageFail("ShadowMapApp::SetupMainCamera", "camera system unavailable");

        main_camera_entity = ecs_context->CreateEntity<Entity>("MainCamera");
        auto camera = main_camera_entity->AddComponent<CameraComponent>();

        camera->control_mode = CameraComponent::ControlMode::ViewModel;
        camera->target = math::Vector3f(0, 0, 0);
        camera->distance = kMainDistance;
        camera->yaw = kMainYaw;
        camera->pitch = kMainPitch;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->camera_data   = GetCamera();
        camera->camera_info   = const_cast<CameraInfo *>(GetCameraInfo());
        camera->viewport_info = GetViewportInfo();

        return true;
    }

public:

    ~ShadowMapApp() override
    {
        SAFE_CLEAR(depth_pass)
        SAFE_CLEAR(scene_vdm)

        if (auto *gc = GetGraphicsContext())
        {
            if (auto *sm = gc->GetManager<SamplerManager>())
            {
                if (scene_sampler)  sm->Release(scene_sampler);
                if (shadow_sampler) sm->Release(shadow_sampler);
            }
        }

        scene_sampler  = nullptr;
        shadow_sampler = nullptr;
    }

    bool Init() override
    {
        LogStage("ShadowMapApp::Init", "begin");

        // 与蓝本一致的背景色
        SetClearColor(Color4f(0.18f, 0.18f, 0.20f, 1.0f));

        ecs_context = GetECSContext();
        if (!ecs_context)
            return LogStageFail("ShadowMapApp::Init", "ECS context is null");

        ecs_context->SetResourceNamePrefix("ShadowMap:MainScene");

        if (!InitVDM())
            return LogStageFail("ShadowMapApp::Init", "InitVDM failed");

        if (!InitMaterialDataSSBO())
            return LogStageFail("ShadowMapApp::Init", "InitMaterialDataSSBO failed");

        if (!InitMaterial())
            return LogStageFail("ShadowMapApp::Init", "InitMaterial failed");

        if (!CreateSceneGeometry())
            return LogStageFail("ShadowMapApp::Init", "CreateSceneGeometry failed");

        auto environment_system = ecs_context->GetSystem<EnvironmentSystem>();
        if (!environment_system)
            environment_system = ecs_context->RegisterRenderSystem<EnvironmentSystem>();

        if (environment_system)
        {
            if (auto *sky = environment_system->EditSkyInfo())
            {
                // 太阳接近正上方（俯仰角 89.5°），与"正俯视的光源相机"保持一致：
                // 影子才会落在物体正下方。见文件头的接收方案说明。
                sky->SetTime(11, 58, 0);
                environment_system->MarkSkyDirty();
            }
        }

        // ── 光源 Pass：把环上网格渲染进 shadow map（不含地面） ──
        depth_pass = new ShadowDepthPass();
        if (!depth_pass->Init(this, kShadowMapSize))
            return LogStageFail("ShadowMapApp::Init", "ShadowDepthPass::Init failed");

        if (!PopulateScene(depth_pass->GetWorld(), "ShadowMapApp::Init:DepthWorld", false, nullptr))
            return LogStageFail("ShadowMapApp::Init", "populate depth world failed");

        if (!CreateLightCamera(depth_pass->GetWorld()))
            return LogStageFail("ShadowMapApp::Init", "CreateLightCamera failed");

        if (!depth_pass->Render())
            return LogStageFail("ShadowMapApp::Init", "ShadowDepthPass::Render failed");

        Texture2D *shadow_map_tex = depth_pass->GetDepthTexture();
        if (!shadow_map_tex)
            return LogStageFail("ShadowMapApp::Init", "shadow map depth texture unavailable");

        // ── 主世界：同一套场景 + 会采样 shadow map 的接收面 ──
        if (!PopulateScene(ecs_context, "ShadowMapApp::Init:MainWorld", true, shadow_map_tex))
            return LogStageFail("ShadowMapApp::Init", "populate main world failed");

        if (!SetupMainCamera())
            return LogStageFail("ShadowMapApp::Init", "SetupMainCamera failed");

        GLogInfo("[ShadowMap][ShadowMapApp::Init] success shadow_map=%p layout=%u size=%ux%u",
                 (void *)shadow_map_tex,
                 (uint32_t)shadow_map_tex->GetImageLayout(),
                 shadow_map_tex->GetWidth(),
                 shadow_map_tex->GetHeight());

        LogStage("ShadowMapApp::Init", "success");
        return true;
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<ShadowMapApp>(OS_TEXT("Shadow Map (depth-only RenderTarget)"), argc, argv, 1280, 720);
}
