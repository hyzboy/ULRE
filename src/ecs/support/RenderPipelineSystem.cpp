#include <hgl/ecs/support/RenderPipelineSystem.h>
#include <hgl/ecs/core/Context.h>

namespace hgl::ecs
{
    bool RenderPipelineSystem::ValidatePipeline(ECSContext* context)
    {
        if (!IsEnabled())
            return false;
        
        if (!context)
            return false;
        
        auto pipeline = GetPipeline(context);
        if (!pipeline)
            return false;
        
        return true;
    }

    // ─────────────────────────────────────────────────────────────
    // CollectSystem
    // ─────────────────────────────────────────────────────────────
    void CollectSystem::Update(float dt)
    {
        if (!ValidatePipeline(context))
            return;
        
        auto pipeline = GetPipeline(context);
        OnCollect(pipeline);
    }

    // ─────────────────────────────────────────────────────────────
    // CullSystem
    // ─────────────────────────────────────────────────────────────
    void CullSystem::Update(float dt)
    {
        if (!ValidatePipeline(context))
            return;
        
        auto pipeline = GetPipeline(context);
        OnCull(pipeline);
    }

    // ─────────────────────────────────────────────────────────────
    // SortSystem
    // ─────────────────────────────────────────────────────────────
    void SortSystem::Update(float dt)
    {
        if (!ValidatePipeline(context))
            return;
        
        auto pipeline = GetPipeline(context);
        OnSort(pipeline);
    }

    // ─────────────────────────────────────────────────────────────
    // BuildSystem
    // ─────────────────────────────────────────────────────────────
    void BuildSystem::Update(float dt)
    {
        if (!ValidatePipeline(context))
            return;
        
        auto pipeline = GetPipeline(context);
        OnBuild(pipeline);
    }

    // ─────────────────────────────────────────────────────────────
    // SyncSystem
    // ─────────────────────────────────────────────────────────────
    void SyncSystem::Update(float dt)
    {
        if (!ValidatePipeline(context))
            return;
        
        auto pipeline = GetPipeline(context);
        OnSync(pipeline);
    }

    // ─────────────────────────────────────────────────────────────
    // RenderPipelineDrawSystem
    // ─────────────────────────────────────────────────────────────
    void RenderPipelineDrawSystem::Render(hgl::graph::RenderCmdBuffer* cmd, float dt)
    {
        if (!ValidatePipeline(context))
            return;
        
        auto pipeline = GetPipeline(context);
        OnRender(pipeline, cmd);
    }

}  // namespace hgl::ecs
