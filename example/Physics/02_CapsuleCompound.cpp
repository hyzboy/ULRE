// 02_CapsuleCompound.cpp —— 范例 2：Z-up 胶囊与复合体沿斜坡滑滚（GUI）
//
// 场景（PhysicsScenarios::BuildCapsuleCompoundScenario）：
//   · 绕 X 轴 -20° 的静态斜坡（高端在 -Y 侧）+ 坡顶动态胶囊 + 坡顶复合「小车」；
//   · 两者应沿 +Y 下滑且 Z 下降 —— 验证 Z-up 约定 + Capsule 局部 Z 轴 + 复合凸包链路；
//   · 空格键：在坡顶再投放一枚新胶囊（旋转对齐坡面法线）。
//
// 视觉说明：胶囊用内建 capsule 非均匀缩放（端盖呈椭球，演示可接受）。

#include "PhysicsAppCommon.h"
#include "../../src/ecs/tests/PhysicsScenarios.h"

#include <hgl/framework/WorkManager.h>
#include <hgl/ecs/systems/tick/InputSystem.h>

#include <random>
#include <cmath>

using namespace hgl;

namespace
{
    class Physics02App final : public WorkObject
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

            sc_ = ecs::BuildCapsuleCompoundScenario(GetECSContext());
            kit_.AttachVisuals(sc_.ground, example::kRowGround);
            kit_.AttachVisuals(sc_.ramp, example::kRowBlue);
            kit_.AttachVisuals(sc_.capsule, example::kRowOrange);
            kit_.AttachVisuals(sc_.compound_cart, example::kRowPurple);
            return kit_.InitCamera(math::Vector3f(0.0f, 3.0f, 1.0f), 16.0f, 60.0f, -20.0f);
        }

        void Tick(double delta) override
        {
            auto input = GetECSContext()->GetSystem<ecs::InputSystem>();
            const bool space = (input != nullptr) && input->IsKeyDown(io::KeyboardButton::Space);
            if(space && !prev_space_)
            {
                DropCapsule();
            }
            prev_space_ = space;
            WorkObject::Tick(delta);
        }

    private:
        void DropCapsule()
        {
            // 与场景内斜坡一致：绕 X 轴 -20°，上表面法线 n=(0,-sinθ,cosθ)，下滑方向 (0,cosθ,sinθ)
            const float kTheta = glm::radians(20.0f);
            const glm::vec3 center(0.0f, 3.0f, 1.5f);
            const glm::vec3 n(0.0f, -std::sin(kTheta), std::cos(kTheta));
            const glm::vec3 down(0.0f, std::cos(kTheta), std::sin(kTheta));
            // angleAxis(+θ, X) 把局部 Z (0,0,1) 转到 n=(0,-sinθ,cosθ)，与斜坡上表面法线一致
            const glm::quat rot = glm::angleAxis(kTheta, glm::vec3(1.0f, 0.0f, 0.0f));

            std::uniform_real_distribution<float> jitter(-0.3f, 0.3f);
            const glm::vec3 pos = center + n * 0.25f + down * (-3.4f) + n * 1.2f
                                  + down * jitter(rng_);

            ecs::PhysicsBodyDesc desc{};
            desc.motion       = ecs::BodyMotionType::Dynamic;
            desc.mass         = 1.5f;
            desc.friction     = 0.2f;
            desc.restitution  = 0.1f;

            ecs::Entity *e = ecs::CreatePhysicsCapsule(GetECSContext(), "Spawned_Capsule",
                                                       math::Vector3f(pos), rot,
                                                       0.4f, 0.8f, desc);
            if(e != nullptr)
            {
                kit_.AttachVisuals(e, kit_.NextDynamicRow());
            }
        }

        example::PhysicsVisualKit kit_;
        std::shared_ptr<ecs::JoltPhysicsSystem> physics_;
        ecs::CapsuleCompoundScenario sc_;
        bool prev_space_ = false;
        std::mt19937 rng_{0xC0FFEEu};
    };
}//namespace

int os_main(int argc, os_char **argv)
{
    return RunFramework<Physics02App>(OS_TEXT("Physics 02 Capsule Compound"), argc, argv, 1280, 720);
}
