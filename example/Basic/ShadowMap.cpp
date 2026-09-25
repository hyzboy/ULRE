#include<hgl/framework/WorkManager.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKTexture.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/render/RenderTargetDesc.h>
#include<hgl/graph/module/RenderTargetManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include<hgl/graph/ssbo/MaterialDataRows.h>

#include<hgl/graph/module/EnvironmentManager.h>
#include<hgl/graph/ubo/SkyInfo.h>
#include<hgl/graph/ubo/ShadowInfo.h>
#include<hgl/graph/camera/ReversedZProj.h>
#include<hgl/vk/VKBindlessTextureManager.h>
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
 *   5. **光源是倾斜的、并且绕场景环绕**（每帧重拍 shadow map）：
 *      仰角 28° → 影子拉长到约 1.88 倍物体高度；方位角 15°/s → 24 秒一圈，
 *      影子跟着在地面上扫过去。
 *   6. **环上网格各自随机自转**（轴/速度/转向都随机，固定种子）。
 *      自转会让几何体最低点下沉，所以每帧按**真实顶点**重算最低点、
 *      同步抬高，形状贴地不穿模。
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
 *            ShadowReceiver 材质源： uv0 → 世界XY → 投影到光源空间 → 深度比较 → 乘 albedo
 *
 * 为了只动"ShaderLibrary + 示例"两层（不改引擎 ABI），光源方向是这样传进 shader 的：
 *
 *   **复用 SkyInfo UBO 里本就存在的 sun_direction，在 shader 中把光源相机整个重建出来。**
 *
 * 具体地：
 *   a. 光源相机是 ViewModel 摆法 —— 位置 = target − forward × distance、target 恒为原点，
 *      所以"相机位置 = +sun_direction × distance、forward = −sun_direction"是它的定义，
 *      不需要额外的光照 VP 矩阵；
 *   b. 右/上向量用与 C++ CameraSystem::ComputeRightUp 完全相同的叉积式子算出
 *      （world_up 固定 (0,0,1)，两侧一致）；
 *   c. 投影用与 MakeInfiniteReversedZProj 同构的式子（fov/近平面是已知常量）；
 *   d. 地面点由 PlaneSquare 的 uv0 反推世界 XY（uv0 = 0.5 + 世界XY / 20）。
 *
 * sun_direction 在 C++ 侧**直接取自光源相机解算出的 forward 取反**，不重复算一套
 * 三角函数 —— 两侧因此不可能对不上。它也同时是主世界方向光的来源
 * （forward_lighting.glsl 的 mainLightDir = GetSkyMainLightDir()），
 * 于是"物体受光方向"与"影子方向"天然一致。
 *
 * 深度比较是 reversed-Z 的标准形式：投影给出 depth = near / lin，
 * 地面**不写进** shadow map，所以未被遮挡的纹素恒为清屏值 0.0f，
 * 判据就是 `采样深度 > 地面自身深度 + bias`。
 *
 * 代价（也就是"只在示例层做"的边界）：
 *   地面必须是平的（uv0 ↔ 世界 XY 是线性关系）。
 *   但光源**不再**需要接近正上方 —— 倾斜、环绕都可以，这正是本次改动拿到的能力。
 *
 * ## TODO：引擎侧要补齐什么（截至本次改动均未实现）
 *
 *   a. **光照空间矩阵**：本用例靠"相机看向原点 + 常量 fov/near"把矩阵解析重建出来，
 *      换成任意光源（点光/聚光/自由朝向的方向光）就必须有真正的 light VP。
 *      Scene UBO 只有 Camera/Sky/Viewport/ColorPalette/GlobalAddresses 五个 binding，
 *      需新增 Shadow UBO（SceneBinding 枚举 + C++ 绑定 + ShaderLibrary/ubo/*.glsl）；
 *   b. **shadow provider**：ShaderLibrary/shadow/ 下只有 identity.glsl，
 *      GetShadowFactor() 恒返回 1.0。需实现按光照空间投影采样 + 深度比较（PCF）；
 *   c. **模板接线**：fragment/forward_lit.glsl.tmpl 的主流程当前**根本不调用**
 *      GetShadowFactor，LightingInput 里也没有 shadow 字段；
 *   d. **材质**：lit.material.toml 需增加 shadow_map 纹理槽；
 *      （现在只有地面用的 ShadowReceiver 材质会采样 shadow map，
 *        所以环上网格**不会**互相投影、也收不到别的网格的影子。
 *        这是本用例最明显的"看起来对、其实不全对"的地方。）
 *
 * ## 验证目标
 *
 *   1. RenderTargetDesc::OffscreenDepthOnly 能建出零颜色附件的 RT
 *      —— color_count == 0、has_depth == true、深度纹理非空
 *   2. depth-only 目标的材质管线能通过 PipelineResolver 的校验并成功创建
 *   3. 渲染结束后深度纹理处于可采样布局（SHADER_READ_ONLY_OPTIMAL）
 *   4. 该深度纹理可以绑定到主世界材质并采样（ShadowReceiver 材质槽 shadow_map）
 *   5. 地面上能看到环上网格投下的、被拉长的影子，且随光源环绕而扫动
 *   6. 每帧重拍 shadow map 之后，地面上的影子与网格当下姿态一致（没有滞后）
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

    // ── 光源视角相机（shadow map 拍摄）：倾斜 + 绕中心环绕 ─────────────

    /// 光源相机到原点的距离。
    static constexpr float  kLightDistance = 36.0f;

    /// 光源仰角（自水平面量起）。
    ///
    /// **倾斜就是为了把影子拉长**：影子长度 ≈ 物体高度 / tan(仰角)。
    ///   仰角 90°（正上方）→ 影子缩在物体正下方（本用例以前的样子）；
    ///   仰角 28°         → 拉长到约 1.88 倍物体高度。
    ///
    /// 不能取 0 或 90：这两种极限下 forward 与 world_up 平行，
    /// cross(forward, world_up) 退化成 0 → right/up 全 NaN。
    /// C++ 的 CameraSystem::ComputeRightUp 与 shader 里重建光源相机的写法
    /// 都有这个奇点，所以仰角固定在中间地带。
    static constexpr float  kLightElevationDeg = 28.0f;

    /// 光源绕世界 Z 轴环绕的角速度（度/秒）。15°/s → 24 秒转一圈。
    static constexpr float  kLightOrbitDegPerSec = 15.0f;

    /// 光源的初始方位角（度）。40° 是刻意选的：既不是正对相机、也不与环上
    /// 任一网格的方位重合，开场第一帧就能看清"影子是斜的"。
    static constexpr float  kLightStartAzimuthDeg = 40.0f;

    /// 光源相机 fov。
    ///
    /// 倾斜之后光锥在地面处的截面**不再等于地面**，所以这里取一个足够宽的值，
    /// 保证无论方位角转到哪里，整块 20×20 地面都落在 [0,1]² 之内。
    ///
    /// 定量依据（地面四角在光源空间的最坏情况）：
    ///   水平方向 |ndc.x| = tan(fov/2)⁻¹ · max|x_v| / lin
    ///                     = (1/0.4663) · 14.14 / 36 = 0.843 < 1
    ///   竖直方向 |ndc.y| ≤ tan(fov/2)⁻¹ · 14.14·sin28° / 23.5 = 0.60 < 1
    /// 50° 对这两项都有约 20% 的余量。
    static constexpr float  kLightFov = 50.0f;

    /// 近平面。reverse-Z + 无限远投影下深度 = near / lin（lin 为沿光轴的视线距离），
    /// near 必须小于场景里离光源最近的几何体。光源倾斜 28° 后，离光源最近的是
    /// 环上朝光源那一侧的网格：
    ///     lin_min ≈ 36 − (8.6·cos28° + 4.2·sin28°) ≈ 26.4
    /// 取 18 留足余量（否则会把网格裁掉一块）。
    static constexpr float  kLightNear = 18.0f;

    /// 远平面。reversed-Z 走 MakeInfiniteReversedZProj，far 不参与运算，
    /// 只写在相机上作为语义说明。
    static constexpr float  kLightFar = 64.0f;

    // ── 环上网格的自转（让场景"活"起来，影子也跟着转） ────────────────

    /// 自转角速度范围（弧度/秒）。每个网格在区间内随机取一个值（含符号）。
    static constexpr float  kMeshSpinSpeedMin = 0.35f;
    static constexpr float  kMeshSpinSpeedMax = 0.95f;

    /// 自转轴 z 分量的取值范围。整个区间为正 → 轴偏"竖立"，
    /// 物体像陀螺一样转，既能明显看出在动，又不会躺倒在地上打滚。
    /// 取 1 附近就是纯绕 Z 转 —— 那对球/半球/圆柱这类轴对称形状是**看不见**的，
    /// 所以下界给到 0.35，让它带一点倾斜。
    static constexpr float  kMeshSpinAxisZMin = 0.35f;
    static constexpr float  kMeshSpinAxisZMax = 1.30f;

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

    /// 方位角 + 固定仰角 → **指向太阳**的单位方向（w 分量为 0 的 Vector4f）。
    ///
    /// 这个向量同时喂给两个地方，且必须完全一致：
    ///   1. SkyInfo::sun_direction —— 主世界方向光的来源（forward_lighting.glsl
    ///      的 mainLightDir = GetSkyMainLightDir()），决定物体的受光方向；
    ///   2. shader 里重建光源相机（shadow_receiver_source.glsl::BuildShadowReceiverLight）。
    ///
    /// 实际上示例并不直接把它写进 sun_direction —— 见 UpdateAnimation：
    /// 那里是先把 yaw/pitch 交给光源相机，再取相机解算出的 forward 取反，
    /// 于是"shader 用的方向"与"相机真实朝向"在浮点层面也完全同源。
    /// 这里保留这个函数是为了在 Init 里给出**初始**方向（那时相机还没 tick 过）。
    ///
    /// 与 CameraSystem::ComputeForward 的关系：
    ///   ComputeForward(yaw, pitch) 给出相机 forward；
    ///   取 yaw = azimuth + 180°、pitch = -elevation，则
    ///     forward = (cosθ·cos(yaw), cosθ·sin(yaw), -sinθ) = -to_sun
    /// 两边互为反向量，正是"相机看向原点、太阳在反方向"的几何关系。
    glm::vec3 LightDirectionFromAzimuth(const float azimuth_deg)
    {
        const float e = glm::radians(kLightElevationDeg);
        const float a = glm::radians(azimuth_deg);

        return glm::vec3(cosf(e) * cosf(a), cosf(e) * sinf(a), sinf(e));
    }

    /// 取出几何体**本地空间**的 Position 顶点（顶点格式固定为 VF_V3F）。
    ///
    /// 用途：环上网格要自转，而自转会让"最低点"下降。要在每一帧把最低点重新
    /// 抬回 z=0，就必须知道**真实顶点**，而不是 AABB 的 8 个角点 ——
    /// 对球、圆环这类内切于 AABB 的形状，角点估计会保守得离谱
    /// （球会浮起来 ≈0.73，圆环会浮起来 ≈2.0，一眼就看出不对）。
    ///
    /// 注意必须在**几何体**上取 VAB 并直接 Map 原始字节：
    ///   * GeometryCreater 在 Create() 返回后已经被 Clear()，拿不到 VAB；
    ///   * VAB::Map(0, count) 给的是**整块缓冲**的首地址，要自己加上
    ///     Geometry::GetVertexOffset() 才是本几何体的第一个顶点。
    std::vector<glm::vec3> ExtractLocalPositions(Geometry *geom)
    {
        std::vector<glm::vec3> out;
        if (!geom)
            return out;

        VAB *vab = geom->GetVAB(VAN::Position);
        if (!vab)
            return out;

        const uint32_t stride = vab->GetStride();
        const uint32_t count  = static_cast<uint32_t>(geom->GetVertexCount());
        const uint32_t offset = static_cast<uint32_t>(geom->GetVertexOffset());

        if (stride < sizeof(float) * 3 || count == 0)
            return out;

        const uint8_t *base = static_cast<const uint8_t *>(vab->Map(0, vab->GetCount()));
        if (!base)
            return out;

        out.reserve(count);
        for (uint32_t i = 0; i < count; ++i)
        {
            const float *p = reinterpret_cast<const float *>(base + static_cast<size_t>(offset + i) * stride);
            out.emplace_back(p[0], p[1], p[2]);
        }

        vab->Unmap();

        return out;
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

    /// 接收面与网格全员使用统一的标准 Lit 材质
    PrimitiveAsset              plane_asset{};

    std::vector<PrimitiveAsset> mesh_assets;

    bool IsValid()const
    {
        return receiver_plane
            && plane_asset.IsValid()
            && !meshes.empty()
            && meshes.size() == mesh_assets.size()
            && meshes.size() == mesh_lift.size();
    }
};

/// 光源视角的 shadow map：**只持有 depth-only RT**，不建 ECS 子世界。
///
/// ## 为什么不用 graph::OffscreenWorld 的 ECS 子世界（历史 + 现状）
///
/// 早期版本这里撞过两个坑，当时选择了"主世界自己渲"的路线：
///
/// 1. **L2W 域互踩（已于引擎层修复）**：TransformAssignmentBuffer 曾把每世界的
///    L2W 缓冲注册进设备级 SSBOBufferRegistry 的全局唯一域（kLocalToWorldAddress），
///    第二个世界注册时 RegisterBuffer 会静默 Release 第一个世界的缓冲——而那个
///    实例仍握着该指针，于是
///        [R11] Failed to register LocalToWorld domain buffer: ... buffer_bytes=0xDDDD…
///    两个世界互相踩、逐帧渲染即崩。该注册自 BDA 化（L2W 地址经 RootAddresses
///    push constants 下发）后已无任何消费者，现已整体摘除；RegisterBuffer 的
///    同域冲突也已改为 fail-fast 拒绝。**子世界方案因此重新可行。**
/// 2. **离屏 RT 命令缓冲同步（仍在）**：离屏 RT 只有一个命令缓冲且默认不等
///    fence，逐帧重拍会撞上上一帧未完成的录制/执行（vkBeginCommandBuffer on
///    active command buffer / VK_ERROR_DEVICE_LOST）。走子世界前需先给离屏 RT
///    补 in-flight 或显式等待（RenderTargetDesc::fence_count 对离屏路径尚未真正生效）。
///
/// 本用例维持"主世界 + RenderTo 切 RT"：两台相机同住一个 CameraSystem，切换
/// 就是置脏 + Update（见 ActivateCamera），且全程只有一份 L2W，不必等第 2 项。
class ShadowDepthPass
{
private:

    graph::RenderTargetHandle rt{};

public:

    ~ShadowDepthPass() = default;

    IRenderTarget *GetRenderTarget() const
    {
        return rt.get();
    }

    Texture2D *GetDepthTexture() const
    {
        IRenderTarget *target = rt.get();
        return target ? target->GetDepthTexture() : nullptr;
    }

    bool Init(GraphicsContext *gc, const uint32_t size)
    {
        LogStage("ShadowDepthPass::Init", "begin");

        if (!gc)
            return LogStageFail("ShadowDepthPass::Init", "graphics context is null");

        auto *rtm = gc->GetRenderTargetManager();
        if (!rtm)
            return LogStageFail("ShadowDepthPass::Init", "render target manager is null");

        // depth-only：零颜色附件，深度渲染后处于可采样布局，可直接绑定采样
        RenderTargetDesc desc = RenderTargetDesc::OffscreenDepthOnly(size,
                                                                     size,
                                                                     "ShadowMap:ShadowMap",
                                                                     PF_D32F);

        rt = rtm->Create(desc);
        if (!rt)
            return LogStageFail("ShadowDepthPass::Init", "RenderTargetManager::Create failed");

        if (rt->GetColorCount() != 0)
        {
            GLogError("[ShadowMap][ShadowDepthPass::Init] expected depth-only target but color_count=%u",
                      rt->GetColorCount());
            return false;
        }

        if (!rt->hasDepth())
            return LogStageFail("ShadowDepthPass::Init", "depth-only target has no depth attachment");

        ReportDepthTarget("ShadowDepthPass::Init", rt.get(), GetDepthTexture(), false);

        LogStage("ShadowDepthPass::Init", "success");
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

    using MaterialDataAccessor = graph::GlobalSSBODataAccessor;
    using MaterialBinding      = hgl::ecs::PrimitiveComponent::MaterialDataAuthoringResource;

    MaterialDataAccessor material_data_ssbo_accessor{};
    graph::ssbo::PBRSurfaceRow scene_material_data{};
    MaterialBinding scene_material_binding{};

    Texture2D *base_texture      = nullptr;
    Texture2D *normal_texture    = nullptr;
    Texture2D *roughness_texture = nullptr;
    Sampler   *scene_sampler     = nullptr;

    uint32_t shadow_map_handle = 0;

    ShadowDepthPass *depth_pass = nullptr;

    // ── 逐帧动画状态 ──────────────────────────────────────────────────

    /// 环上网格的逐帧动画状态（自转 + 落地位移补偿）。
    struct MeshAnim
    {
        /// 自转轴（本地空间单位向量，整体偏竖立）
        glm::vec3            spin_axis{0.0f, 0.0f, 1.0f};

        /// 自转角速度（弧度/秒，带符号 → 转向有正有反）
        float                spin_speed = 0.0f;

        /// 网格在环上的静态朝向（均布角）
        glm::quat            base_rotation{1.0f, 0.0f, 0.0f, 0.0f};

        /// 静止时的落地位移（把 AABB 最低点抬到 z=0）
        float                rest_lift = 0.0f;

        /// 环上的 XY 位置（z 由落地位移决定）
        glm::vec3            ring_pos{0.0f};

        /// 本地空间顶点（相对网格原点）。逐帧用它算旋转后的真实最低点。
        std::vector<glm::vec3> verts;

        /// 主世界里的 TransformComponent（由 ECSContext 持有，裸指针即可）。
        /// 注意 shadow map 由**主世界自己**渲染（见 RenderShadowMap），
        /// 所以只需要这一份 —— 不再有"离屏世界那份也要同步"的问题。
        TransformComponent * tf = nullptr;
    };

    std::vector<MeshAnim> mesh_anim;

    /// 主世界的相机系统
    std::shared_ptr<CameraSystem> camera_system;

    /// 两台相机都在主世界里：
    ///   - main_camera：可见画面用，由 SetupMainCamera 创建；
    ///   - light_camera：拍 shadow map 用，由 CreateLightCamera 创建。
    /// 相机 UBO 的契约是"每个 RT/RenderPass 开始时全量写入"
    /// （见 CameraSystem::CommitCameraUBO 的注释），所以 shadow pass 只要在
    /// RenderTo 之前手动把光源相机的矩阵写进 camera_info 即可，
    /// 不需要 toggling is_main_camera。
    std::shared_ptr<CameraComponent> main_camera;
    std::shared_ptr<CameraComponent> light_camera;

    PrimitiveComponent *receiver_prim = nullptr;

    /// 主世界的环境系统与它的 SkyInfo 数据。sun_direction 从这里写进去。
    std::shared_ptr<EnvironmentSystem> environment_system;
    graph::SkyInfo *                   sky_info = nullptr;

    /// 动画累计时间（秒）。用它算方位角与自转角，而不是依赖固定帧率。
    double anim_time = 0.0;

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
        auto *domain_manager = GetManager<GlobalSSBOBufferRegistry>();
        if (!domain_manager)
            return LogStageFail("ShadowMapApp::InitMaterialDataSSBO", "material ssbo registry is null");

        // 与 BasicLitMeshes 相同的 PBR 参数
        scene_material_data.base_color   = Color4f(1.0f);
        scene_material_data.metallic     = 0.08f;
        scene_material_data.roughness    = 0.92f;
        scene_material_data.normal_scale = 0.35f;

        material_data_ssbo_accessor = domain_manager->GetAccessor<graph::ssbo::PBRSurfaceRow>();
        if (!material_data_ssbo_accessor)
            return LogStageFail("ShadowMapApp::InitMaterialDataSSBO", "create accessor failed");

        if (!material_data_ssbo_accessor.Write(scene_material_data))
            return LogStageFail("ShadowMapApp::InitMaterialDataSSBO", "write material data failed");

        scene_material_binding = material_data_ssbo_accessor.GetGlobalSSBOBinding();
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

        // ── 全场景统一使用标准 Lit 材质（地面与环上网格共用，自然支持投射与接收阴影） ──
        scene_recipe.recipe_name = "ShadowMap.Scene";
        scene_recipe.mtl_def_id  = "Lit";
        scene_recipe.render_state_overrides.pipeline_config = mtl::MakeSolid3DConfig();
        if (!(scene_recipe.material_ssbo_binding = scene_material_binding).IsValid())
            return LogStageFail("ShadowMapApp::InitMaterial", "scene recipe material SSBO binding invalid");

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

        // ── 组装资产（地面与网格全员使用统一的 Lit 材质配方） ──
        scene.plane_asset = PrimitiveAsset(scene.receiver_plane, &scene_recipe, PrimitiveType::Triangles);
        if (!scene.plane_asset.IsValid())
            return LogStageFail("ShadowMapApp::CreateSceneGeometry", "create receiver plane asset failed");

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

    /// 给实体挂标准 Lit 材质（砖墙 base_color/normal/roughness + PBR 参数）。
    /// 地面（is_receiver_plane）只挂 base_color，避免 20x 平铺糊掉法线；网格挂全套贴图。
    void ApplyMeshMaterial(PrimitiveComponent *prim, bool is_receiver_plane = false)
    {
        if (!prim)
            return;

        prim->SetMaterialTextureResource("base_color", base_texture, scene_sampler);
        if (!is_receiver_plane)
        {
            prim->SetMaterialTextureResource("normal", normal_texture, scene_sampler);
            prim->SetMaterialTextureResource("roughness", roughness_texture, scene_sampler);
        }
        prim->SetMaterialDataResource(scene_material_binding);
        prim->SetVisible(true);
    }

    /// 把场景铺进主世界（接收面 + 环上网格）。
    bool PopulateScene(ECSContext *world, const char *stage, const bool include_receiver)
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

            prim_comp->SetPrimitiveAsset(&scene.plane_asset);
            ApplyMeshMaterial(prim_comp.get(), /*is_receiver_plane=*/true);
            receiver_prim = prim_comp.get();
        }

        // ── 环上网格：与蓝本相同的排布，额外按 AABB 抬到地面之上 ──
        // Mobility 用 Movable：这些网格每帧都要自转。L2W 只有在出现动态 transform
        // 时才会走"每帧环形段"那条路径，静态槽则只做脏数据重传。
        //
        // 注：把场景复制进 OffscreenWorld 子世界过去因 L2W 全局域互踩而不可行，
        // 该根源已在引擎层修复（见 ShadowDepthPass 顶部说明）。本用例仍维持
        // "主世界自己渲染 shadow map"：相机切换与同步语义更直接。
        const size_t count = scene.meshes.size();
        for (size_t i = 0; i < count; ++i)
        {
            auto *entity = world->CreateEntity<Entity>("Mesh_" + std::to_string(i));
            auto transform = entity->AddComponent<TransformComponent>(Mobility::Movable);
            auto prim_comp = entity->AddComponent<PrimitiveComponent>();

            glm::vec3 pos = RingPosition(i, count);
            pos.z += scene.mesh_lift[i];

            transform->SetLocalPosition(pos);
            transform->SetLocalRotation(RingRotation(i, count));
            transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));

            // 记下 Transform，供逐帧自转写入
            if (i < mesh_anim.size())
                mesh_anim[i].tf = transform.get();

            prim_comp->SetPrimitiveAsset(&scene.mesh_assets[i]);
            ApplyMeshMaterial(prim_comp.get());
        }

        GLogInfo("[ShadowMap][%s] populated %zu meshes + receiver(%d)",
                 stage, count, include_receiver ? 1 : 0);
        return true;
    }

    /// 定量校验：把地面四角按 **shader 里同一套公式** 投影一遍，打印 uv 范围。
    ///
    /// 为什么值得打印：倾斜光源后光锥截面不再等于地面，如果某个方位角下地面
    /// 有角点跑到 [0,1]² 之外，shader 会把它按"不受遮挡"处理 —— 表现就是
    /// 贴图边界上出现一条**没有影子的接缝**。这个日志能直接判定 fov 够不够。
    ///
    /// 环上网格都在半径 8.6 之内、且比地面更靠近光源，所以"地面四角"就是
    /// 最坏情况，不需要再单独检查网格。
    void LogLightCoverage(const float azimuth_deg) const
    {
        const float e = glm::radians(kLightElevationDeg);
        const float a = glm::radians(azimuth_deg);

        const glm::vec3 to_sun(cosf(e) * cosf(a), cosf(e) * sinf(a), sinf(e));
        const glm::vec3 fwd   = -to_sun;
        const glm::vec3 pos   = to_sun * kLightDistance;
        const glm::vec3 right = glm::normalize(glm::cross(fwd, glm::vec3(0.0f, 0.0f, 1.0f)));
        const glm::vec3 up    = glm::normalize(glm::cross(right, fwd));

        const float f = 1.0f / tanf(glm::radians(kLightFov) * 0.5f);

        glm::vec2 uv_min(1.0e9f);
        glm::vec2 uv_max(-1.0e9f);
        float     lin_min = 1.0e9f;
        float     lin_max = -1.0e9f;

        for (int sy = -1; sy <= 1; sy += 2)
        {
            for (int sx = -1; sx <= 1; sx += 2)
            {
                const glm::vec3 ground(sx * kReceiverHalfExtent, sy * kReceiverHalfExtent, 0.0f);
                const glm::vec3 rel = ground - pos;
                const float     lin = glm::dot(rel, fwd);

                if (lin < lin_min) lin_min = lin;
                if (lin > lin_max) lin_max = lin;

                const glm::vec2 uv = glm::vec2(0.5f) + 0.5f * glm::vec2(f * glm::dot(rel, right) / lin,
                                                                        -f * glm::dot(rel, up) / lin);

                uv_min.x = (uv.x < uv_min.x) ? uv.x : uv_min.x;
                uv_min.y = (uv.y < uv_min.y) ? uv.y : uv_min.y;
                uv_max.x = (uv.x > uv_max.x) ? uv.x : uv_max.x;
                uv_max.y = (uv.y > uv_max.y) ? uv.y : uv_max.y;
            }
        }

        const bool inside = (uv_min.x >= 0.0f) && (uv_min.y >= 0.0f)
                         && (uv_max.x <= 1.0f) && (uv_max.y <= 1.0f);

        GLogInfo("[ShadowMap][LightCoverage] azimuth=%.1f 地面四角 uv=[%.3f,%.3f]..[%.3f,%.3f] lin=[%.2f,%.2f] inside=%d",
                 azimuth_deg, uv_min.x, uv_min.y, uv_max.x, uv_max.y, lin_min, lin_max, inside ? 1 : 0);

        if (!inside)
            GLogError("[ShadowMap][LightCoverage] 地面超出光源视锥：请调大 kLightFov 或 kLightDistance");
    }

    /// 按方位角摆放光源相机。初始与逐帧都走这里，保证两条路径不会走偏。
    ///
    /// 关键关系（与 shader 的 BuildShadowReceiverLight 必须一致）：
    ///   相机看向 -to_sun ⇒ forward 的欧拉角就是 pitch = -elevation、yaw = azimuth + 180°
    /// 推导：ComputeForward(yaw,pitch) = (cosθ·cos(yaw), cosθ·sin(yaw), sinθ)，其中 θ = -pitch。
    /// 代入 yaw = azimuth + 180° 得 forward = (-cosθ·cos(az), -cosθ·sin(az), -sinθ) = -to_sun ✓
    void PlaceLightCamera(const float azimuth_deg)
    {
        if (!light_camera)
            return;

        const float e = glm::radians(kLightElevationDeg);
        const float a = glm::radians(azimuth_deg);
        const glm::vec3 to_sun(cosf(e) * cosf(a), cosf(e) * sinf(a), sinf(e));

        light_camera->target       = math::Vector3f(0, 0, 0);
        light_camera->distance     = kLightDistance;
        light_camera->position     = to_sun * kLightDistance;
        light_camera->world_up     = math::Vector3f(0, 0, 1);
        light_camera->yaw          = azimuth_deg + 180.0f;
        light_camera->pitch        = -kLightElevationDeg;
        light_camera->fov          = kLightFov;
        light_camera->near_plane   = kLightNear;
        light_camera->far_plane    = kLightFar;
        light_camera->matrix_dirty = true;
    }

    /// 光源视角相机：由方位角决定朝向，**倾斜**看向原点（target 恒为原点）。
    ///
    /// 它和主相机一样住在**主世界**里，但 is_main_camera = false ——
    /// CameraSystem::SelectMainCamera 只认主相机，所以它不会被误用；
    /// shadow pass 由 RenderShadowMap 把它手动推成"当帧生效的相机"。
    ///
    /// 摆法由"阴影接收方案"决定（详见文件头）：
    ///   - 仰角固定 kLightElevationDeg（倾斜 → 影子拉长）；
    ///   - 方位角逐帧环绕 → 影子绕场景扫动；
    ///   - fov 取足够宽，保证任意方位角下整块地面都在 [0,1]² 内；
    ///   - near 收紧到 18（见 kLightNear 注释）。
    bool CreateLightCamera(ECSContext *world)
    {
        if (!world)
            return LogStageFail("ShadowMapApp::CreateLightCamera", "world is null");

        if (!camera_system)
            return LogStageFail("ShadowMapApp::CreateLightCamera", "camera system unavailable");

        auto *entity = world->CreateEntity<Entity>("LightCamera");
        auto camera = entity->AddComponent<CameraComponent>();

        camera->control_mode   = CameraComponent::ControlMode::ViewModel;
        camera->is_main_camera = false;

        light_camera = camera;

        PlaceLightCamera(kLightStartAzimuthDeg);

        GLogInfo("[ShadowMap][ShadowMapApp::CreateLightCamera] distance=%.2f elevation=%.1f azimuth=%.1f fov=%.4f near=%.2f far=%.2f",
                 kLightDistance, kLightElevationDeg, kLightStartAzimuthDeg, kLightFov,
                 kLightNear, kLightFar);

        LogLightCoverage(kLightStartAzimuthDeg);

        return true;
    }

    /// 用光源相机把**主世界**渲染进 shadow map。
    ///
    /// 通过 RenderPassRequest 携带 light_camera 覆盖：
    ///   1. `ECSContext::RenderTo` 内部自动将 RenderTargetSystem 切到 shadow RT 并同步 1024x1024 视口；
    ///   2. CameraSystem 在 pass 期间只解算 light_camera 写入共享 camera_info（不响应输入）；
    ///   3. pass 结束后自动还原主 RT、主视口并重新解算主相机。
    bool RenderShadowMap()
    {
        auto *rt = depth_pass ? depth_pass->GetRenderTarget() : nullptr;

        if (!rt || !ecs_context)
            return LogStageFail("ShadowMapApp::RenderShadowMap", "depth target / ecs context missing");

        // if (receiver_prim)
        //     receiver_prim->SetVisible(false);

        ecs::RenderPassRequest req;
        req.target           = rt;
        req.camera           = light_camera.get();
        req.use_target_clear = true;
        req.cull_mode        = ecs::CullMode::Front; // 阴影贴图渲染模型背面（显式声明，无隐式推断）

        const bool ok = ecs_context->RenderTo(req);

        // if (receiver_prim)
        //     receiver_prim->SetVisible(true);

        if (!ok)
            return LogStageFail("ShadowMapApp::RenderShadowMap", "ECSContext::RenderTo failed");

        return true;
    }

    /// 初始化环上网格的逐帧动画参数（自转轴 / 角速度 / 本地顶点 / 静止落地位移）。
    ///
    /// **必须在 PopulateScene 之前调用**：PopulateScene 会把两个世界的
    /// TransformComponent* 回填进 mesh_anim。
    bool InitMeshAnimation()
    {
        const size_t count = scene.meshes.size();
        if (count == 0 || scene.mesh_lift.size() != count)
            return LogStageFail("ShadowMapApp::InitMeshAnimation", "scene meshes are not ready");

        mesh_anim.assign(count, MeshAnim{});

        // 固定种子的 LCG。**刻意不用 std::random**：结果每次运行完全一致，
        // 截图才能和上一版做像素级比对。
        uint32_t seed = 0x9E3779B9u;
        auto rnd01 = [&seed]() -> float
        {
            seed = seed * 1664525u + 1013904223u;
            return static_cast<float>(seed >> 8) / static_cast<float>(1u << 24);
        };

        for (size_t i = 0; i < count; ++i)
        {
            MeshAnim &anim = mesh_anim[i];

            // 自转轴：xy 随机方向、z 取正区间 → 整体竖立但明显倾斜。
            // 倾斜是必需的：纯绕 Z 转对球/圆柱/圆环这类轴对称形状**看不见**。
            const float ax = rnd01() * 2.0f - 1.0f;
            const float ay = rnd01() * 2.0f - 1.0f;
            const float az = kMeshSpinAxisZMin + rnd01() * (kMeshSpinAxisZMax - kMeshSpinAxisZMin);

            glm::vec3 axis(ax, ay, az);
            if (glm::length(axis) < 1.0e-4f)
                axis = glm::vec3(0.0f, 0.0f, 1.0f);

            anim.spin_axis = glm::normalize(axis);

            // 角速度：区间内随机，且正负各半 —— 有正转有反转才不会像队列操练
            anim.spin_speed = kMeshSpinSpeedMin + rnd01() * (kMeshSpinSpeedMax - kMeshSpinSpeedMin);
            if (rnd01() < 0.5f)
                anim.spin_speed = -anim.spin_speed;

            anim.base_rotation = RingRotation(i, count);
            anim.rest_lift     = scene.mesh_lift[i];
            anim.ring_pos      = RingPosition(i, count);
            anim.verts         = ExtractLocalPositions(scene.meshes[i]);

            GLogInfo("[ShadowMap][ShadowMapApp::InitMeshAnimation] mesh[%zu] axis=(%.3f,%.3f,%.3f) speed=%.3f rad/s verts=%zu rest_lift=%.3f",
                     i, anim.spin_axis.x, anim.spin_axis.y, anim.spin_axis.z,
                     anim.spin_speed, anim.verts.size(), anim.rest_lift);
        }

        return true;
    }

    /// 把光源相机解算出的朝向写进 SkyInfo::sun_direction（取反 = 指向太阳）。
    ///
    /// 为什么绕这一圈而不是直接写三角函数算出来的方向：这样"shader 重建的
    /// 光源相机"与"C++ 里真正的相机"在浮点层面完全同源，不可能出现
    /// "两个方向差了 1e-6、只在贴图边缘露出来"的怪问题。
    ///
    /// 必须在 RenderShadowMap() **之后**调用 —— 光源相机的矩阵与 forward
    /// 是 RenderTo(req.camera) 在里面解算的。
    void SyncSunDirectionFromLightCamera()
    {
        if (!light_camera || !environment_system || !sky_info)
            return;

        const glm::vec3 to_sun = -light_camera->forward;

        sky_info->sun_direction = math::Vector4f(to_sun.x, to_sun.y, to_sun.z, 0.0f);
        environment_system->MarkSkyDirty();
    }

    /// 逐帧推进：光源环绕 → 重拍 shadow map → 网格自转（两个世界同步写入）。
    ///
    /// 为什么放在这里而不是 wo->Render()：ECS 帧里给 wo->Render 的 pre_render
    /// 回调是在 **BeginManagedRenderFrame 之后**才调的（此时命令缓冲已开），
    /// 只适合往里录 draw；而重跑 shadow map 要另起一帧（begin/end/submit），
    /// 只能在帧外做。Tick 相位正好在 Render 之前、且没有开帧。
    void UpdateAnimation(const double delta)
    {
        anim_time += delta;

        const float azimuth = kLightStartAzimuthDeg
                            + kLightOrbitDegPerSec * static_cast<float>(anim_time);
        const float t = static_cast<float>(anim_time);

        // ── 1) 网格自转 + 落地位移补偿 ──
        // 必须**先**更新姿态再重拍 shadow map，否则影子会比网格慢一帧。
        for (MeshAnim &anim : mesh_anim)
        {
            const glm::quat spin = glm::angleAxis(anim.spin_speed * t, anim.spin_axis);
            const glm::quat rot  = anim.base_rotation * spin;

            // 旋转后几何体在本地空间的最低点 z。
            // 顶点取不到（VAB 异常）时退回"按静止姿态"处理，不影响其它网格。
            float min_z = 0.0f;
            if (!anim.verts.empty())
            {
                const glm::mat3 m = glm::mat3_cast(rot);

                min_z = 1.0e9f;
                for (const glm::vec3 &v : anim.verts)
                {
                    const float z = (m * v).z;
                    if (z < min_z)
                        min_z = z;
                }
            }

            // 静止时 rest_lift 恰好把最低点放到 z=0；自转后（最低点会下沉）按需再抬。
            // 用真实顶点而不是 AABB 角点，轴对称形状因此**完全不会**上下浮动。
            const float lift = (anim.rest_lift > -min_z) ? anim.rest_lift : -min_z;

            glm::vec3 pos = anim.ring_pos;
            pos.z += lift;

            if (anim.tf)
            {
                anim.tf->SetLocalPosition(pos);
                anim.tf->SetLocalRotation(rot);
            }
        }

        // ── 2) 光源环绕：摆好相机 → 重拍 shadow map ──
        PlaceLightCamera(azimuth);

        if (!RenderShadowMap())
            GLogError("[ShadowMap][ShadowMapApp::UpdateAnimation] RenderShadowMap failed");

        // ── 3) 把相机真实的 forward 同步给 SkyInfo（主世界的方向光也随之转） ──
        SyncSunDirectionFromLightCamera();

        // ── 4) 同步 ShadowInfo（光源 VP 矩阵、参数、尺寸、纹理句柄） ──
        SyncShadowInfo();
    }

    /// 将光源空间 View-Projection 矩阵及阴影控制参数同步至全局 ShadowInfo UBO
    void SyncShadowInfo()
    {
        if (!light_camera || !environment_system)
            return;

        auto *shadow_info = environment_system->EditShadowInfo();
        if (!shadow_info)
            return;

        const float fov_rad = glm::radians(light_camera->fov);
        const float aspect  = 1.0f; // 正方形 1024x1024 shadow map
        const math::Matrix4f proj = MakeInfiniteReversedZProj(fov_rad, aspect, light_camera->near_plane);
        const math::Matrix4f view = math::LookAtMatrix(light_camera->position, light_camera->target, light_camera->world_up);

        shadow_info->shadow_vp           = proj * view;
        shadow_info->shadow_params       = math::Vector4f(0.002f, 1.5f, 0.12f, 0.0f);
        shadow_info->shadow_map_size     = math::Vector2f(static_cast<float>(kShadowMapSize), static_cast<float>(kShadowMapSize));
        shadow_info->inv_shadow_map_size = math::Vector2f(1.0f / static_cast<float>(kShadowMapSize), 1.0f / static_cast<float>(kShadowMapSize));
        shadow_info->shadow_tex          = math::Vector4u(shadow_map_handle, 0, 0, 0);

        environment_system->MarkShadowDirty();
    }

    bool SetupMainCamera()
    {
        if (!ecs_context || !ecs_context->EnsureCameraSystem())
            return LogStageFail("ShadowMapApp::SetupMainCamera", "camera system unavailable");

        // 主世界只有这一个 CameraSystem，两台相机（主 + 光源）都由它驱动
        camera_system = ecs_context->GetSystem<CameraSystem>();
        if (!camera_system)
            return LogStageFail("ShadowMapApp::SetupMainCamera", "camera system is null");

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

        main_camera = camera;

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
            }
        }

        scene_sampler  = nullptr;
    }

    /// 逐帧推进：光源环绕 + 重拍 shadow map + 网格自转。
    ///
    /// 位置很关键 —— 必须在 **Tick**（帧之前、无命令缓冲）而不是 wo->Render()
    /// （ECS 的 pre_render 回调，已经 BeginManagedRenderFrame，只能录 draw）。
    /// 详见 UpdateAnimation 的注释。
    void Tick(double delta) override
    {
        // 基类实现把 delta 转给 ECSContext，主世界的 Tick 相位（主相机、环境、
        // Transform…）都在那里跑。**必须调**，否则主相机矩阵不再更新。
        WorkObject::Tick(delta);

        UpdateAnimation(delta);
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

        // 网格动画参数要在 PopulateScene 之前备好（后者会回填 Transform 指针）
        if (!InitMeshAnimation())
            return LogStageFail("ShadowMapApp::Init", "InitMeshAnimation failed");

        // 相机系统要在建光源相机之前就绪（光源相机也是它的相机之一）
        if (!ecs_context->EnsureCameraSystem())
            return LogStageFail("ShadowMapApp::Init", "camera system unavailable");

        camera_system = ecs_context->GetSystem<CameraSystem>();
        if (!camera_system)
            return LogStageFail("ShadowMapApp::Init", "camera system is null");

        // 先跑一次，让 CameraSystem 物化它的 camera_ubo / camera_info
        camera_system->Update(0.0f);

        // 把系统级 viewport 固定成主画面尺寸：它只在为 null 时才会去 latch
        // （见 CameraSystem::Update 里的 `if (!viewport_info)`），
        // 而 shadow pass 期间世界的 render_target 是**离屏 RT** ——
        // 一旦让它在那里 latch，主画面的宽高比就变成 1 了。
        if (auto *main_rt = ecs_context->GetRenderTarget())
            camera_system->SetViewportInfo(main_rt->GetViewportInfo());

        environment_system = ecs_context->GetSystem<EnvironmentSystem>();
        if (!environment_system)
            environment_system = ecs_context->RegisterRenderSystem<EnvironmentSystem>();

        if (!environment_system)
            return LogStageFail("ShadowMapApp::Init", "environment system unavailable");

        sky_info = environment_system->EditSkyInfo();
        if (!sky_info)
            return LogStageFail("ShadowMapApp::Init", "edit sky info failed");

        // 时间刻意配到与光源仰角一致的那一档：SetTime 的模型里
        //   仰角 = (小时 − 6) × 15°
        // 取 7:52 → 仰角恰好 28°，与 kLightElevationDeg 对齐。
        // 这样太阳颜色/强度也是"这个仰角该有的样子"，而不是正午的白光。
        // （SetTime 写进去的 sun_direction 随后会被真实的光源相机朝向覆盖。）
        sky_info->SetTime(7, 52, 0);
        environment_system->MarkSkyDirty();

        // ── shadow map：只建一个 depth-only RT，**不建第二个 ECS 世界** ──
        depth_pass = new ShadowDepthPass();
        if (!depth_pass->Init(GetGraphicsContext(), kShadowMapSize))
            return LogStageFail("ShadowMapApp::Init", "ShadowDepthPass::Init failed");

        // 逐帧重拍的同步由引擎内 RenderTo 保证：每次离屏提交完成后等该 RT
        // 自己的 queue fence（微秒级），既避免下一帧重录单命令缓冲时撞上
        // 未完成的上一笔提交，也保证主帧采样 shadow map 时 GPU 已完成写入。
        // （旧方案是在主 ECSContext 上 SetWaitIdleEnabled(true)——每帧两次
        // 全设备排空，连坐主渲染，已随引擎侧 fence 等待移除。）

        Texture2D *shadow_map_tex = depth_pass->GetDepthTexture();
        if (!shadow_map_tex)
            return LogStageFail("ShadowMapApp::Init", "shadow map depth texture unavailable");

        auto *btm = GetGraphicsContext()->GetBindlessTextureManager();
        if (!btm)
            return LogStageFail("ShadowMapApp::Init", "bindless texture manager is null");

        shadow_map_handle = btm->RegisterTexture(shadow_map_tex);
        if (shadow_map_handle == 0)
            return LogStageFail("ShadowMapApp::Init", "register shadow map texture failed");

        GLogInfo("[ShadowMap][ShadowMapApp::Init] registered shadow map bindless handle=%u", shadow_map_handle);

        // ── 主世界：接收面 + 环上网格 + 两台相机（全员统一使用标准 Lit 材质） ──
        if (!PopulateScene(ecs_context, "ShadowMapApp::Init:MainWorld", true))
            return LogStageFail("ShadowMapApp::Init", "populate main world failed");

        if (!SetupMainCamera())
            return LogStageFail("ShadowMapApp::Init", "SetupMainCamera failed");

        if (!CreateLightCamera(ecs_context))
            return LogStageFail("ShadowMapApp::Init", "CreateLightCamera failed");

        // 第一帧之前就要有正确的 shadow map 与 sun_direction 以及 ShadowInfo：
        // 主循环里 Tick 在 Render 之前，而 WorkManager 的**第一次** Tick 会因为
        // delta < frame_time 被跳过，所以不能指望 UpdateAnimation 来补这一发。
        if (!RenderShadowMap())
            return LogStageFail("ShadowMapApp::Init", "RenderShadowMap failed");

        SyncSunDirectionFromLightCamera();
        SyncShadowInfo();

        GLogInfo("[ShadowMap][ShadowMapApp::Init] success shadow_map=%p layout=%u size=%ux%u sun=(%.3f,%.3f,%.3f)",
                 (void *)shadow_map_tex,
                 (uint32_t)shadow_map_tex->GetImageLayout(),
                 shadow_map_tex->GetWidth(),
                 shadow_map_tex->GetHeight(),
                 sky_info->sun_direction.x, sky_info->sun_direction.y, sky_info->sun_direction.z);

        LogStage("ShadowMapApp::Init", "success");
        return true;
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<ShadowMapApp>(OS_TEXT("Shadow Map (depth-only RenderTarget)"), argc, argv, 1280, 720);
}
