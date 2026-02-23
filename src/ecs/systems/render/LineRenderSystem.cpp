#include<hgl/ecs/systems/render/LineRenderSystem.h>
#include<hgl/ecs/systems/render/RenderPrimitiveSubmitSystem.h>
#include<hgl/ecs/systems/render/LineCollectSystem.h>
#include<hgl/ecs/components/LinesComponent.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/graph/geo/line/LineRenderManager.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/VKRenderTarget.h>

namespace hgl::ecs
{
    LineRenderSystem::LineRenderSystem(const std::string &name)
        : System(name)
    {
        SetSystemType(SystemType::RenderSubmit);
        SetExecutionOrder(ExecutionPhase::RenderBatch);
        SetRenderElementType("Line");
        AddDependency<RenderPrimitiveSubmitSystem>();
    }

    LineRenderSystem::~LineRenderSystem()
    {
        Shutdown();
    }

    void LineRenderSystem::Initialize()
    {
        if (manager_initialized || !context)
            return;

        auto *rt = context->GetRenderTarget();
        if (!rt)
            return;

        if (!line_manager)
        {
            if (auto *rc = context->GetRenderContext())
                line_manager = graph::CreateLineRenderManager(rc, rt);
            else if (auto *gc = context->GetGraphicsContext())
                line_manager = graph::CreateLineRenderManager(gc, rt);

            own_line_manager = (line_manager != nullptr);
        }

        manager_initialized = (line_manager != nullptr);
    }

    void LineRenderSystem::Shutdown()
    {
        if (line_manager && own_line_manager)
            delete line_manager;

        line_manager        = nullptr;
        own_line_manager    = false;
        manager_initialized = false;
        last_batch_frame_   = UINT32_MAX;
    }

    void LineRenderSystem::SetColor(int index, const hgl::Color4f &color)
    {
        if (!line_manager && context && !manager_initialized)
            Initialize();

        if (line_manager)
            line_manager->SetColor(index, color);
    }

    uint32_t LineRenderSystem::GetLineCount() const
    {
        return line_manager ? line_manager->GetLineCount() : 0;
    }

    void LineRenderSystem::Update(float /*deltaTime*/)
    {
        // Frame guard: PrepareRenderPassSetup already ran Collect+Batch before the upload.
        // The render pass re-invokes Update() for systems in the phase range, but we must
        // not re-dirty the StagedBuffer after the upload has already happened (vkCmdCopyBuffer
        // is forbidden inside a Vulkan render pass).
        if (context)
        {
            const uint32_t frame = context->GetFrameIndex();
            if (frame == last_batch_frame_)
                return;
            last_batch_frame_ = frame;
        }

        if (!line_manager)
            Initialize();
        if (!line_manager || !context)
            return;

        // Reset all batches for this frame
        line_manager->ClearLines();

        // Gather visible line components
        std::vector<std::shared_ptr<LinesComponent>> components;
        if (auto collect_sp = context->GetSystem<LineCollectSystem>())
            components = collect_sp->GetVisibleComponents();
        else
            context->GetComponents<LinesComponent>(components);

        // Write component lines into StagedBuffer VABs.
        // AddLine() → LineWidthBatch::AddLine() → BufferAccessor::Write() marks StagedBuffer dirty.
        // RenderBufferUploadSystem auto-uploads all dirty StagedBuffers before RenderDrawSubmit.
        for (const auto &comp : components)
        {
            if (!comp || !comp->visible || comp->lines.empty())
                continue;

            std::vector<LineSegmentDescriptor> descs;
            descs.reserve(comp->lines.size());
            for (const auto &seg : comp->lines)
                descs.push_back({seg.from, seg.to, seg.color_index});

            line_manager->AddLine(comp->width, descs);
            comp->MarkSynced();
        }
    }

    void LineRenderSystem::Render(graph::RenderCmdBuffer *cmd, float /*deltaTime*/)
    {
        if (!cmd || !line_manager)
            return;

        line_manager->Draw(cmd);
    }
}//namespace hgl::ecs
