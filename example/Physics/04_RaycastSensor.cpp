// 04_RaycastSensor.cpp —— 范例 4：射线击飞与传感器区域高亮（GUI）
//
// 场景（PhysicsScenarios::BuildRaycastSensorScenario）：
//   · 地面散落 4 枚动态靶箱（其中一枚初始位于 3x3x3 传感器体积内）；
//   · 左键按下边沿：从鼠标位置发射世界射线，命中靶箱即施加击飞冲量
//     （RaycastAndKnockback：Jolt RayCastShape + 刚体 AddImpulse）；
//   · 每帧 QuerySensorOverlap（Jolt OverlapShape）——区内靶箱变红、区外变白，
//     验证传感器 is_sensor 不阻挡运动但参与重叠查询。
//
// 注：ViewModel 相机左键也用于旋转视角；击飞取「按下边沿」单次触发，
// 拖动转视角时不会连发，仅起始瞬间可能顺带击中一枚，属演示可接受。

#include "PhysicsAppCommon.h"
#include "../../src/ecs/tests/PhysicsScenarios.h"

#include <hgl/framework/WorkManager.h>
#include <hgl/ecs/systems/tick/InputSystem.h>

#include <algorithm>

using namespace hgl;

namespace
{
    class Physics04App final : public WorkObject
    {
    public:
        bool Init() override
        {
            SetClearColor(Color4f(0.10f, 0.12f, 0.16f, 1.0f));
            if(!kit_.Init(this))
            {
                return false;
            }
            physics_ = kit_.EnsurePhysics();
            if(physics_ == nullptr)
            {
                return false;
            }

            sc_ = ecs::BuildRaycastSensorScenario(GetECSContext());
            kit_.AttachVisuals(sc_.ground, example::kRowGround);
            kit_.AttachVisuals(sc_.sensor, example::kRowCyan, /*wire=*/true);
            for(ecs::Entity *target : sc_.targets)
            {
                kit_.AttachVisuals(target, example::kRowWhite);
            }
            return kit_.InitCamera(math::Vector3f(2.0f, 0.0f, 1.0f), 18.0f, 45.0f, -25.0f);
        }

        void Tick(double delta) override
        {
            auto input = GetECSContext()->GetSystem<ecs::InputSystem>();
            const bool left = (input != nullptr) && input->IsMouseButtonDown(io::MouseButton::Left);
            if(left && !prev_left_)
            {
                FireRay();
            }
            prev_left_ = left;

            UpdateSensorHighlight();
            WorkObject::Tick(delta);
        }

    private:
        void FireRay()
        {
            math::Ray ray{};
            if(!kit_.PickRay(ray) || physics_ == nullptr)
            {
                return;
            }
            // 冲量 = 射线方向水平分量放大 + 上抛分量：命中即「击飞」
            const math::Vector3f impulse(ray.direction * 40.0f
                                         + math::Vector3f(0.0f, 0.0f, 12.0f));
            ecs::Entity *hit = ecs::RaycastAndKnockback(physics_.get(), ray.origin,
                                                         ray.direction, 200.0f, impulse);
            if(hit != nullptr)
            {
                kit_.SetVisualRow(hit, example::kRowSalmon);
            }
        }

        void UpdateSensorHighlight()
        {
            if(physics_ == nullptr)
            {
                return;
            }
            inside_.clear();
            ecs::QuerySensorOverlap(physics_.get(), sc_, inside_);
            for(ecs::Entity *target : sc_.targets)
            {
                const bool in_sensor = std::find(inside_.begin(), inside_.end(), target)
                                       != inside_.end();
                kit_.SetVisualRow(target, in_sensor ? example::kRowRed : example::kRowWhite);
            }
        }

        example::PhysicsVisualKit kit_;
        std::shared_ptr<ecs::JoltPhysicsSystem> physics_;
        ecs::RaycastSensorScenario sc_;
        std::vector<ecs::Entity *> inside_;
        bool prev_left_ = false;
    };
}//namespace

int os_main(int argc, os_char **argv)
{
    return RunFramework<Physics04App>(OS_TEXT("Physics 04 Raycast Sensor"), argc, argv, 1280, 720);
}
