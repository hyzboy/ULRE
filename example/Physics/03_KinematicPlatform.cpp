// 03_KinematicPlatform.cpp —— 范例 3：运动学平台正弦往复推挤（GUI）
//
// 场景（PhysicsScenarios::BuildKinematicPlatformScenario）：
//   · Kinematic 平台沿 Y 做正弦往复（幅度 4），搭载数枚动态箱体，+Y 远端有静态挡墙；
//   · ECS 侧每帧 DriveKinematicPlatform(t) 写平台变换，物理系统在随后的 Update 中
//     以 MoveKinematic 同步 —— 验证「ECS -> 运动学单向驱动」与推挤/拖拽摩擦链路。
//
// 时序要点：Drive 必须在 WorkObject::Tick（内部跑 ECS 系统 Update）**之前**调用，
// 保证平台位移与骑手推挤发生在同一帧。

#include "PhysicsAppCommon.h"
#include "../../src/ecs/tests/PhysicsScenarios.h"

#include <hgl/framework/WorkManager.h>

using namespace hgl;

namespace
{
    class Physics03App final : public WorkObject
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

            sc_ = ecs::BuildKinematicPlatformScenario(GetECSContext());
            kit_.AttachVisuals(sc_.ground, example::kRowGround);
            kit_.AttachVisuals(sc_.platform, example::kRowYellow);
            kit_.AttachVisuals(sc_.push_wall, example::kRowBlue);
            for(ecs::Entity *rider : sc_.riders)
            {
                kit_.AttachVisuals(rider, kit_.NextDynamicRow());
            }
            return kit_.InitCamera(math::Vector3f(0.0f, 3.0f, 1.0f), 22.0f, 90.0f, -30.0f);
        }

        void Tick(double delta) override
        {
            sim_time_ += static_cast<float>(delta);
            ecs::DriveKinematicPlatform(GetECSContext(), sc_, sim_time_);
            WorkObject::Tick(delta);
        }

    private:
        example::PhysicsVisualKit kit_;
        std::shared_ptr<ecs::JoltPhysicsSystem> physics_;
        ecs::KinematicPlatformScenario sc_;
        float sim_time_ = 0.0f;
    };
}//namespace

int os_main(int argc, os_char **argv)
{
    return RunFramework<Physics03App>(OS_TEXT("Physics 03 Kinematic Platform"), argc, argv, 1280, 720);
}
