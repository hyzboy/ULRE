#include <hgl/ecs/support/RenderPipelineSystem.h>
#include <hgl/ecs/core/Context.h>

namespace hgl::ecs
{
    bool RenderPipelineSystem::ValidatePipeline(ECSContext* context)
    {
        return GetValidatedPipeline(context) != nullptr;
    }

    RenderPipelineBase* RenderPipelineSystem::GetValidatedPipeline(ECSContext* context)
    {
        if (!IsEnabled() || !context)
            return nullptr;

        return GetPipeline(context);
    }

    // ─────────────────────────────────────────────────────────────
    // CollectSystem
    // ─────────────────────────────────────────────────────────────
    void CollectSystem::Update(float dt)
    {
        auto pipeline = GetValidatedPipeline(context);
        if (!pipeline)
            return;

        OnCollect(pipeline);
    }

    // ─────────────────────────────────────────────────────────────
    // CullSystem
    // ─────────────────────────────────────────────────────────────
    void CullSystem::Update(float dt)
    {
        auto pipeline = GetValidatedPipeline(context);
        if (!pipeline)
            return;

        OnCull(pipeline);
    }

    // ─────────────────────────────────────────────────────────────
    // SortSystem
    // ─────────────────────────────────────────────────────────────
    void SortSystem::Update(float dt)
    {
        auto pipeline = GetValidatedPipeline(context);
        if (!pipeline)
            return;

        OnSort(pipeline);
    }

    // ─────────────────────────────────────────────────────────────
    // BuildSystem
    // ─────────────────────────────────────────────────────────────
    void BuildSystem::Update(float dt)
    {
        auto pipeline = GetValidatedPipeline(context);
        if (!pipeline)
            return;

        OnBuild(pipeline);
    }

    // ─────────────────────────────────────────────────────────────
    // SyncSystem
    // ─────────────────────────────────────────────────────────────
    void SyncSystem::Update(float dt)
    {
        auto pipeline = GetValidatedPipeline(context);
        if (!pipeline)
            return;

        OnSync(pipeline);
    }

    // ─────────────────────────────────────────────────────────────
    // RenderPipelineDrawSystem
    // ─────────────────────────────────────────────────────────────
    void RenderPipelineDrawSystem::Render(hgl::graph::RenderCmdBuffer* cmd, float dt)
    {
        auto pipeline = GetValidatedPipeline(context);
        if (!pipeline)
            return;

        OnRender(pipeline, cmd);
    }

}  // namespace hgl::ecs
