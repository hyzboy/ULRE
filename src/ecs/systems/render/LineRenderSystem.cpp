#include<hgl/ecs/systems/render/LineRenderSystem.h>
#include<hgl/ecs/systems/render/RenderPrimitiveSubmitSystem.h>
#include<hgl/ecs/systems/render/LineCollectSystem.h>
#include<hgl/ecs/components/LinesComponent.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/graph/geo/line/LineRenderManager.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKDevice.h>
#include<algorithm>

namespace hgl::ecs
{
    uint64_t LineRenderSystem::MakeComponentKey(const LinesComponent* comp) const
    {
        return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(comp));
    }

    LineRenderSystem::LineRenderSystem(const std::string &name)
        : System(name)
    {
        SetSystemType(SystemType::RenderSubmit);
        SetExecutionOrder(ExecutionPhase::RenderDebug);
        SetRenderElementType("Line");
        AddDependency<RenderPrimitiveSubmitSystem>();
    }

    void LineRenderSystem::Initialize()
    {
        if (manager_initialized || !context)
            return;

        auto *rt = context->GetRenderTarget();
        if (!rt)
            return;

        manager_initialized = true;
    }

    void LineRenderSystem::SetColor(int index, const hgl::Color4f &color)
    {
        if (!line_manager && context && !manager_initialized)
        {
            Initialize();
        }

        if (line_manager)
            line_manager->SetColor(index, color);
    }

    void LineRenderSystem::SyncComponentsToRenderer()
    {
        if (!line_manager || !context)
            return;

        std::vector<std::shared_ptr<LinesComponent>> components;
        LineCollectSystem* collect_system = nullptr;

        if (auto collect_system_sp = context->GetSystem<LineCollectSystem>())
        {
            collect_system = collect_system_sp.get();
            components = collect_system->GetVisibleComponents();

            if (has_uploaded_once
                && collect_system->GetVisibleDirtyCount() == 0
                && collect_system->GetVisibleSetSignature() == last_collect_visible_set_signature)
            {
                return;
            }
        }
        else
        {
            context->GetComponents<LinesComponent>(components);
        }

        std::unordered_set<uint64_t> new_active_keys;
        uint32_t uploaded_line_count = 0;

        for (const auto &comp : components)
        {
            if (!comp || !comp->visible || comp->lines.empty())
                continue;

            const uint64_t key = MakeComponentKey(comp.get());
            new_active_keys.insert(key);

            if (has_uploaded_once && !comp->dirty && active_component_keys.find(key) != active_component_keys.end())
            {
                uploaded_line_count += static_cast<uint32_t>(comp->lines.size());
                continue;
            }

            std::vector<LineSegmentDescriptor> line_list;
            line_list.reserve(comp->lines.size());
            for (const auto &line : comp->lines)
            {
                LineSegmentDescriptor desc;
                desc.from = line.from;
                desc.to = line.to;
                desc.color = line.color_index;
                line_list.push_back(desc);
            }

            line_manager->UpsertComponentLines(key, comp->width, line_list);
            uploaded_line_count += static_cast<uint32_t>(line_list.size());

            comp->MarkSynced();
        }

        if (has_uploaded_once)
        {
            for (const uint64_t key : active_component_keys)
            {
                if (new_active_keys.find(key) == new_active_keys.end())
                    line_manager->RemoveComponentLines(key);
            }
        }

        line_manager->CommitComponentLines();

        active_component_keys.swap(new_active_keys);
        has_uploaded_once = true;
        last_uploaded_line_count = uploaded_line_count;

        if (collect_system)
            last_collect_visible_set_signature = collect_system->GetVisibleSetSignature();
    }

    void LineRenderSystem::Render(graph::RenderCmdBuffer *cmd, float /*deltaTime*/)
    {
        if (!cmd || !line_manager)
            return;

        SyncComponentsToRenderer();
        line_manager->Draw(cmd);
    }
}//namespace hgl::ecs
