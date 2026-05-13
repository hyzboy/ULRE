#include<hgl/ecs/systems/render/PrimitiveBindingCommitSystem.h>
#include<hgl/ecs/systems/render/TextureMaterialBindingSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/log/Log.h>

namespace hgl::ecs
{
    PrimitiveBindingCommitSystem::PrimitiveBindingCommitSystem(const std::string &name)
        : System(name)
    {
        SetSystemType(SystemType::ShaderMaterialProgram);
        SetExecutionOrder(ExecutionPhase::RenderMaterialBind);
        SetRenderElementType("Primitive");
        AddDependency<TextureMaterialBindingSystem>();
    }

    void PrimitiveBindingCommitSystem::Update(float /*deltaTime*/)
    {
        if (!world)
            return;

        std::vector<std::shared_ptr<PrimitiveComponent>> primitives;
        world->GetComponents<PrimitiveComponent>(primitives);

        uint32_t committed_count = 0;
        uint32_t skipped_no_staging = 0;

        for (auto &primitive : primitives)
        {
            if (!primitive)
                continue;

            const auto &staging = primitive->GetStagingRenderState();
            if (!staging.ready)
            {
                ++skipped_no_staging;
                continue;

            }

            if (primitive->CommitStagingRenderState())
                ++committed_count;
        }

        if (!primitives.empty())
        {
            LogInfo("[ECS::PrimitiveBindingCommitSystem] summary: total=%u committed=%u skipped_no_staging=%u",
                    static_cast<unsigned>(primitives.size()),
                    committed_count,
                    skipped_no_staging);
        }
    }
}//namespace hgl::ecs
