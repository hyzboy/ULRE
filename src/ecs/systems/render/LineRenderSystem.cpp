#include<hgl/ecs/systems/render/LineRenderSystem.h>
#include<hgl/ecs/systems/render/RenderPrimitiveSubmitSystem.h>
#include<hgl/ecs/components/LinesComponent.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/graph/geo/line/LineRenderManager.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKDevice.h>

namespace hgl::ecs
{
    LineRenderSystem::LineRenderSystem(const std::string &name)
        : System(name)
    {
        SetSystemType(SystemType::RenderSubmit);
        SetExecutionOrder(ExecutionPhase::RenderPostProcess_LineRenderSystem);
        AddDependency<RenderPrimitiveSubmitSystem>();
    }

    void LineRenderSystem::Initialize()
    {
        if (manager_initialized || !context)
            return;

        auto *rt = context->GetRenderTarget();
        if (!rt)
            return;

        // CN: 在这里暂时不创建 Manager，等到有需要时创建
        // EN: Don't create manager here yet, will create on first use
        // 实际创建会在 Render 方法中延迟进行
        manager_initialized = true;
    }

    void LineRenderSystem::SetColor(int index, const hgl::Color4f &color)
    {
        // CN: 如果还没有 Manager，尝试延迟创建
        // EN: If no manager yet, try to create on-demand
        if (!line_manager && context && !manager_initialized)
        {
            Initialize();
            // 如果 Initialize 还是没有创建 Manager，我们需要等待用户设置
        }

        if (line_manager)
            line_manager->SetColor(index, color);
    }

    void LineRenderSystem::SyncComponentsToRenderer()
    {
        if (!line_manager || !context)
            return;

        // CN: 清空上一帧的线条数据
        // EN: Clear previous frame's line data
        line_manager->ClearLines();

        // CN: 遍历所有 LinesComponent 并同步
        // EN: Iterate all LinesComponent and sync
        std::vector<std::shared_ptr<LinesComponent>> components;
        context->GetComponents<LinesComponent>(components);

        for (const auto &comp : components)
        {
            if (!comp || !comp->visible || comp->lines.empty())
                continue;

            // CN: 将每个 LinesComponent 的线条添加到对应宽度的批次
            // EN: Add LinesComponent's lines to the corresponding width batch
            for (const auto &line : comp->lines)
            {
                line_manager->AddLine(line.from, line.to, line.color_index, comp->width);
            }

            comp->MarkSynced();
        }
    }

    void LineRenderSystem::Render(graph::RenderCmdBuffer *cmd, float /*deltaTime*/)
    {
        if (!cmd || !line_manager)
            return;

        // CN: 每帧同步 Component 数据到渲染器
        // EN: Sync component data to renderer every frame
        SyncComponentsToRenderer();

        // CN: 执行绘制
        // EN: Execute draw
        line_manager->Draw(cmd);
    }
}//namespace hgl::ecs

