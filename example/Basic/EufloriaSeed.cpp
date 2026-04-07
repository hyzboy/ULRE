// EufloriaSeed.cpp
// 绘制类似游戏《Eufloria》种子（seed）的小例子
// A small example that draws a seed similar to those in the game Eufloria
//
// 本范例展示了：
// 1. 程序化生成椭圆（中心种子体）和叶形花瓣的顶点数据（顶点颜色内嵌）
// 2. 共享几何体（Geometry），中心体与花瓣分别烘焙各自颜色，共用同一语义材质 ID
// 3. 使用 Movable Transform 在每帧更新花瓣的位置与旋转，实现整体旋转动画
// 4. 所有关键参数均以 constexpr 常量暴露，方便自定义外观
//
// 参数说明：
//   BG_*          背景颜色（RGB）
//   BODY_SEGMENTS 中心体多边形段数（越大越圆）
//   BODY_RADIUS_X / BODY_RADIUS_Y  中心体 X/Y 半径（NDC 坐标，不等时为椭圆）
//   BODY_*        中心体颜色（RGBA，烘焙至顶点）
//   PETAL_COUNT   花瓣数量
//   PETAL_OFFSET  花瓣根部到中心的距离
//   PETAL_LENGTH  花瓣长度
//   PETAL_WIDTH   花瓣最大宽度
//   PETAL_SEGMENTS 花瓣轮廓段数（越大越光滑）
//   PETAL_*       花瓣颜色（RGBA，烘焙至顶点）
//   ROTATION_SPEED 旋转速度（弧度/秒），设为 0 禁用旋转

#include<hgl/framework/WorkManager.h>
#include<hgl/vk/VKVertexInputConfig.h>
#include<hgl/graph/module/MaterialAssetRegistry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/geo/GraphicsGeometryFactory.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/color/Color.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/systems/tick/TransformSystem.h>
#include<hgl/ecs/systems/render/RenderPrimitiveCollectSystem.h>

#include<cmath>
#include<numbers>
#include<vector>
#include<string>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

//=============================================================================
// 可自定义参数 (Customizable Parameters)
//=============================================================================

// 背景颜色 (Background color, RGB 0–1)
constexpr float BG_R = 0.04f, BG_G = 0.04f, BG_B = 0.12f;

// 中心种子体参数 (Seed body parameters)
constexpr int   BODY_SEGMENTS  = 32;      // 多边形段数，越大越圆 (polygon segments, higher = rounder)
constexpr float BODY_RADIUS_X  = 0.10f;   // X 半径（NDC）(X radius in NDC)
constexpr float BODY_RADIUS_Y  = 0.075f;  // Y 半径（NDC）(Y radius; differs from X for an ellipse)
constexpr float BODY_R         = 0.95f;   // 中心体颜色 R
constexpr float BODY_G         = 0.72f;   // 中心体颜色 G
constexpr float BODY_B         = 0.18f;   // 中心体颜色 B
constexpr float BODY_A         = 1.00f;   // 中心体颜色 A

// 花瓣参数 (Petal parameters)
constexpr int   PETAL_COUNT    = 7;       // 花瓣数量 (number of petals)
constexpr float PETAL_OFFSET   = 0.12f;   // 花瓣根部到中心的距离 (distance from center to petal base)
constexpr float PETAL_LENGTH   = 0.26f;   // 花瓣长度 (petal length in NDC)
constexpr float PETAL_WIDTH    = 0.07f;   // 花瓣最大宽度 (maximum petal half-width)
constexpr int   PETAL_SEGMENTS = 8;       // 花瓣轮廓段数，越大越光滑 (segments for smooth leaf profile)
constexpr float PETAL_R        = 0.25f;   // 花瓣颜色 R
constexpr float PETAL_G        = 0.72f;   // 花瓣颜色 G
constexpr float PETAL_B        = 0.35f;   // 花瓣颜色 B
constexpr float PETAL_A        = 1.00f;   // 花瓣颜色 A

// 动画参数 (Animation parameters)
constexpr float ROTATION_SPEED = 0.25f;   // 旋转速度（弧度/秒），0 = 禁用旋转 (rotation speed rad/s, 0 = static)

//=============================================================================

class EufloriaSeedApp : public WorkObject
{
private:

    ECSContext*         ecs_world          = nullptr;

    SemanticMaterialId  semantic_material_id = 0;   // 顶点色材质（body/petal 共用）

    Primitive*          prim_body  = nullptr;   // 椭圆（中心种子体）
    Primitive*          prim_petal = nullptr;   // 叶形花瓣

    // 中心体实体
    Entity*             body_entity    = nullptr;
    TransformComponent* body_transform = nullptr;

    // 花瓣实体数组
    struct PetalData
    {
        Entity*             entity     = nullptr;
        TransformComponent* transform  = nullptr;
        float               base_angle = 0.0f;   // 该花瓣在初始状态下的角度（弧度）
    };

    PetalData petals[PETAL_COUNT];

    double elapsed_time = 0.0;

private:

    bool InitMaterial()
    {
        static const mtl::MaterialAssetRecord kEufloriaCfg {
            .id       = "eufloria_seed_vertex_color",
            .preset   = mtl::MaterialPreset::VertexColor2D,
            .dim      = mtl::MaterialAssetRecord::Dim::D2,
            .pipeline = GraphicsPipelinePreset::Solid2D,
            .mi_vil_overrides =
            {
                { VAN::Color, VF_V4UN8 },
            },
        };

        semantic_material_id = RegisterSemanticMaterial(kEufloriaCfg);
        return semantic_material_id != 0;
    }

    // 将 0–1 float color 转换为 uint8 RGBA 辅助函数
    static uint8 ToU8(float v) { return static_cast<uint8>(v * 255.0f + 0.5f); }

    // 生成椭圆几何体，顶点内嵌中心体颜色（三角形扇面展开为三角形列表）
    bool InitBodyGeometry()
    {
        const int   num_verts = BODY_SEGMENTS * 3;
        const float step      = 2.0f * std::numbers::pi_v<float> / BODY_SEGMENTS;

        std::vector<float>  positions;
        std::vector<uint8>  colors;
        positions.reserve(num_verts * 2);
        colors.reserve(num_verts * 4);

        const uint8 cr = ToU8(BODY_R), cg = ToU8(BODY_G),
                    cb = ToU8(BODY_B), ca = ToU8(BODY_A);

        auto pushColor = [&]{ colors.insert(colors.end(), {cr, cg, cb, ca}); };

        for (int i = 0; i < BODY_SEGMENTS; i++)
        {
            float a0 = step * i;
            float a1 = step * (i + 1);

            // 中心点
            positions.push_back(0.0f);
            positions.push_back(0.0f);
            pushColor();
            // 第 i 个轮廓点
            positions.push_back(BODY_RADIUS_X * std::cos(a0));
            positions.push_back(BODY_RADIUS_Y * std::sin(a0));
            pushColor();
            // 第 i+1 个轮廓点
            positions.push_back(BODY_RADIUS_X * std::cos(a1));
            positions.push_back(BODY_RADIUS_Y * std::sin(a1));
            pushColor();
        }

        prim_body = GraphicsGeometryFactory::CreatePrimitive(
                        GetGraphicsContext(),
                        semantic_material_id,
                        "SeedBody",
                        num_verts,
                        {{VAN::Position, VF_V2F,   positions.data()},
                         {VAN::Color,    VF_V4UN8, colors.data()}});
        return prim_body != nullptr;
    }

    // 生成叶形花瓣几何体，顶点内嵌花瓣颜色（+Y 方向，根部在原点，sin 曲线轮廓）
    bool InitPetalGeometry()
    {
        // 每段产生 2 个三角形；左右轮廓各 PETAL_SEGMENTS 段
        const int num_verts = PETAL_SEGMENTS * 2 * 3;

        // 计算 PETAL_SEGMENTS+1 对轮廓点（左/右）
        std::vector<float> lx(PETAL_SEGMENTS + 1), ly(PETAL_SEGMENTS + 1);
        std::vector<float> rx(PETAL_SEGMENTS + 1), ry(PETAL_SEGMENTS + 1);

        for (int j = 0; j <= PETAL_SEGMENTS; j++)
        {
            float t     = (float)j / PETAL_SEGMENTS;
            float w     = PETAL_WIDTH * std::sin(t * std::numbers::pi_v<float>);
            float y_pos = t * PETAL_LENGTH;

            lx[j] = -w;  ly[j] = y_pos;
            rx[j] =  w;  ry[j] = y_pos;
        }

        std::vector<float> positions;
        std::vector<uint8> colors;
        positions.reserve(num_verts * 2);
        colors.reserve(num_verts * 4);

        const uint8 cr = ToU8(PETAL_R), cg = ToU8(PETAL_G),
                    cb = ToU8(PETAL_B), ca = ToU8(PETAL_A);

        auto pushPos   = [&](float x, float y){ positions.push_back(x); positions.push_back(y); };
        auto pushColor = [&]{ colors.insert(colors.end(), {cr, cg, cb, ca}); };

        for (int j = 0; j < PETAL_SEGMENTS; j++)
        {
            // 左侧三角形
            pushPos(lx[j],   ly[j]);   pushColor();
            pushPos(lx[j+1], ly[j+1]); pushColor();
            pushPos(rx[j],   ry[j]);   pushColor();

            // 右侧三角形
            pushPos(lx[j+1], ly[j+1]); pushColor();
            pushPos(rx[j+1], ry[j+1]); pushColor();
            pushPos(rx[j],   ry[j]);   pushColor();
        }

        prim_petal = GraphicsGeometryFactory::CreatePrimitive(
                         GetGraphicsContext(),
                         semantic_material_id,
                         "SeedPetal",
                         num_verts,
                         {{VAN::Position, VF_V2F,   positions.data()},
                          {VAN::Color,    VF_V4UN8, colors.data()}});
        return prim_petal != nullptr;
    }

    // 根据当前旋转角度更新第 idx 个花瓣的变换
    void UpdatePetalTransform(int idx, float rotation)
    {
        TransformComponent* t = petals[idx].transform;
        if (!t) return;

        float angle = petals[idx].base_angle + rotation;

        // 花瓣根部位置：以 (sin, cos) 方向分布（0° 在 +Y，顺时针增大）
        float px = PETAL_OFFSET * std::sin(angle);
        float py = PETAL_OFFSET * std::cos(angle);
        t->SetLocalPosition(glm::vec3(px, py, 0.0f));

        // 旋转使花瓣尖端朝外：
        //   花瓣几何体尖端在 +Y 方向；绕 Z 轴旋转 -angle 后，+Y 将指向
        //   方向 (sin(angle), cos(angle))，即朝向外侧。
        glm::quat rot = glm::angleAxis(-angle, glm::vec3(0.0f, 0.0f, 1.0f));
        t->SetLocalRotation(rot);
        t->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        t->MarkDirty();
    }

    bool InitECS()
    {
        ecs_world = GetECSContext();
        if (!ecs_world)
            return false;

        // 中心体实体（Movable，每帧更新旋转）
        body_entity    = ecs_world->CreateEntity<Entity>("SeedBody");
        body_transform = body_entity->AddComponent<TransformComponent>(Mobility::Movable).get();
        body_transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        body_transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        body_transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));

        auto body_prim_comp = body_entity->AddComponent<hgl::ecs::PrimitiveComponent>();
        body_prim_comp->SetPrimitive(prim_body);
        body_prim_comp->SetSemanticMaterial(semantic_material_id);
        body_prim_comp->SetVisible(true);

        // 花瓣实体（Movable，共享花瓣图元，各自有独立变换）
        for (int i = 0; i < PETAL_COUNT; i++)
        {
            petals[i].base_angle = (2.0f * std::numbers::pi_v<float> / PETAL_COUNT) * i;

            petals[i].entity    = ecs_world->CreateEntity<Entity>("SeedPetal_" + std::to_string(i));
            petals[i].transform = petals[i].entity->AddComponent<TransformComponent>(Mobility::Movable).get();

            UpdatePetalTransform(i, 0.0f);   // 初始化变换

            auto prim_comp = petals[i].entity->AddComponent<hgl::ecs::PrimitiveComponent>();
            prim_comp->SetPrimitive(prim_petal);
            prim_comp->SetSemanticMaterial(semantic_material_id);
            prim_comp->SetVisible(true);
        }

        if (auto render_collect = ecs_world->GetSystem<RenderPrimitiveCollectSystem>())
            render_collect->SetSemanticRuntimeResolveEnabled(true);

        return true;
    }

public:

    using WorkObject::WorkObject;

    bool Init() override
    {
        SetClearColor(Color4f(BG_R, BG_G, BG_B, 1.0f));

        if (!InitMaterial())
            return false;

        if (!InitBodyGeometry())
            return false;

        if (!InitPetalGeometry())
            return false;

        if (!InitECS())
            return false;

        return true;
    }

    void Tick(double delta_time) override
    {
        elapsed_time += delta_time;

        const float rotation = static_cast<float>(elapsed_time * ROTATION_SPEED);

        // 中心椭圆体随种子整体缓慢旋转
        glm::quat body_rot = glm::angleAxis(rotation, glm::vec3(0.0f, 0.0f, 1.0f));
        body_transform->SetLocalRotation(body_rot);
        body_transform->MarkDirty();

        // 更新所有花瓣的位置与旋转
        for (int i = 0; i < PETAL_COUNT; i++)
            UpdatePetalTransform(i, rotation);

        // 驱动 TransformSystem 计算最新的世界变换矩阵
        if (auto ts = ecs_world->GetSystem<TransformSystem>())
            ts->Update(static_cast<float>(delta_time));

        WorkObject::Tick(delta_time);
    }

}; // class EufloriaSeedApp

int os_main(int, os_char**)
{
    return RunFramework<EufloriaSeedApp>(OS_TEXT("Eufloria Seed"), 800, 800);
}
