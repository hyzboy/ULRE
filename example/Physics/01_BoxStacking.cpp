// 01_BoxStacking.cpp —— 范例 1：箱体堆叠与弹性反弹（GUI）
//
// 场景（PhysicsScenarios::BuildBoxStackingScenario）：
//   · 40x40 静态地面 + 3-2-1 金字塔静摩擦堆叠 + 两枚高恢复系数落箱；
//   · 空格键：在随机 (x,y) 的 z=12 高度再投放一枚弹性落箱。
//
// 验证点：堆叠箱体稳定静置不穿透、落箱触地反弹、新箱随机散落。

#include "PhysicsAppCommon.h"
#include "../../src/ecs/tests/PhysicsScenarios.h"

#include <hgl/framework/WorkManager.h>
#include <hgl/ecs/systems/tick/InputSystem.h>

#include <random>

using namespace hgl;

namespace
{
    class Physics01App final : public WorkObject
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

            ecs::ECSContext *ctx = GetECSContext();
            sc_ = ecs::BuildBoxStackingScenario(ctx);
            kit_.AttachVisuals(sc_.ground, example::kRowGround);
            for(ecs::Entity *box : sc_.stack_boxes)
            {
                kit_.AttachVisuals(box, kit_.NextDynamicRow());
            }
            for(ecs::Entity *box : sc_.bouncy_boxes)
            {
                kit_.AttachVisuals(box, example::kRowCrimson);
            }
            return kit_.InitCamera(math::Vector3f(0.0f, 0.0f, 1.5f), 20.0f, 45.0f, -25.0f);
        }

        void Tick(double delta) override
        {
            auto input = GetECSContext()->GetSystem<ecs::InputSystem>();
            const bool space = (input != nullptr) && input->IsKeyDown(io::KeyboardButton::Space);
            if(space && !prev_space_)
            {
                SpawnBouncy();
            }
            prev_space_ = space;
            WorkObject::Tick(delta);
        }

    private:
        void SpawnBouncy()
        {
            std::uniform_real_distribution<float> dist(-8.0f, 8.0f);
            ecs::PhysicsBodyDesc desc{};
            desc.motion         = ecs::BodyMotionType::Dynamic;
            desc.mass           = 2.0f;
            desc.friction       = 0.4f;
            desc.restitution    = 0.8f;
            desc.linear_damping = 0.01f;
            desc.allow_sleeping = false;

            const math::Vector3f pos(dist(rng_), dist(rng_), 12.0f);
            ecs::Entity *e = ecs::CreatePhysicsBox(GetECSContext(), "Spawned_Bouncy",
                                                   pos, glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                                                   math::Vector3f(0.5f, 0.5f, 0.5f), desc);
            if(e != nullptr)
            {
                kit_.AttachVisuals(e, example::kRowYellow);
            }
        }

        example::PhysicsVisualKit kit_;
        std::shared_ptr<ecs::JoltPhysicsSystem> physics_;
        ecs::BoxStackingScenario sc_;
        bool prev_space_ = false;
        std::mt19937 rng_{0xD1E5u};
    };
}//namespace

int os_main(int argc, os_char **argv)
{
    return RunFramework<Physics01App>(OS_TEXT("Physics 01 Box Stacking"), argc, argv, 1280, 720);
}
