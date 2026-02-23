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
#include<hgl/log/Log.h>

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
        SetExecutionOrder(ExecutionPhase::RenderBatch);  // Data sync in RenderBatch phase
        SetRenderElementType("Line");
        AddDependency<RenderPrimitiveSubmitSystem>();
    }

    LineRenderSystem::~LineRenderSystem()
    {
        Shutdown();
    }

    void LineRenderSystem::Initialize()
    {
        LogDebug("[LineRenderSystem::Initialize] START manager_initialized=%d context=%p", manager_initialized, context);
        
        if (manager_initialized || !context)
        {
            LogDebug("[LineRenderSystem::Initialize] EARLY_RETURN: manager_initialized=%d or no context", manager_initialized);
            return;
        }

        auto *rt = context->GetRenderTarget();
        LogDebug("[LineRenderSystem::Initialize] RenderTarget=%p", rt);
        if (!rt)
        {
            LogDebug("[LineRenderSystem::Initialize] EARLY_RETURN: no RenderTarget");
            return;
        }

        if (!line_manager)
        {
            if (auto *render_context = context->GetRenderContext())
            {
                LogDebug("[LineRenderSystem::Initialize] Creating LineRenderManager via RenderContext");
                line_manager = graph::CreateLineRenderManager(render_context, rt);
            }
            else if (auto *graphics_context = context->GetGraphicsContext())
            {
                LogDebug("[LineRenderSystem::Initialize] Creating LineRenderManager via GraphicsContext");
                line_manager = graph::CreateLineRenderManager(graphics_context, rt);
            }
            else
            {
                LogError("[LineRenderSystem::Initialize] No RenderContext or GraphicsContext available");
            }

            own_line_manager = (line_manager != nullptr);
            LogDebug("[LineRenderSystem::Initialize] LineRenderManager created=%p own=%d", line_manager, own_line_manager);
        }

        manager_initialized = true;
        first_render_after_init = true;  // Force first render to always sync
        LogDebug("[LineRenderSystem::Initialize] COMPLETE, first_render_after_init set");
    }

    void LineRenderSystem::Shutdown()
    {
        if (line_manager && own_line_manager)
            delete line_manager;

        line_manager = nullptr;
        own_line_manager = false;
        manager_initialized = false;
        first_render_after_init = true;  // Reset for next initialization
        has_uploaded_once = false;
        active_component_keys.clear();
        last_uploaded_line_count = 0;
        last_collect_visible_set_signature = 0;
        last_synced_frame_index = UINT32_MAX;
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

    void LineRenderSystem::Update(float deltaTime)
    {
        LogDebug("[LineRenderSystem::Update] START (RenderBatch phase) has_uploaded_once=%d", has_uploaded_once);
        
        if (!line_manager)
        {
            LogDebug("[LineRenderSystem::Update] line_manager is null, calling Initialize()");
            Initialize();
        }
        
        if (!line_manager)
        {
            LogError("[LineRenderSystem::Update] line_manager still null after Initialize()");
            return;
        }

        // Sync LinesComponent data to LineRenderManager in RenderBatch phase.
        // This runs BEFORE RenderBufferCommit/RenderBufferUpload phases,
        // so data writes to geometry VAB will be picked up by the automatic buffer upload system.
        LogDebug("[LineRenderSystem::Update] Calling SyncComponentsToRenderer()");
        SyncComponentsToRenderer();
        LogDebug("[LineRenderSystem::Update] COMPLETE");
    }

    void LineRenderSystem::SyncComponentsToRenderer()
    {
        LogDebug("[LineRenderSystem::SyncComponentsToRenderer] START has_uploaded_once=%d", has_uploaded_once);
        
        if (!line_manager || !context)
        {
            LogError("[LineRenderSystem::SyncComponentsToRenderer] line_manager=%p or context=%p is null", line_manager, context);
            return;
        }

        std::vector<std::shared_ptr<LinesComponent>> components;
        LineCollectSystem* collect_system = nullptr;

        if (auto collect_system_sp = context->GetSystem<LineCollectSystem>())
        {
            collect_system = collect_system_sp.get();
            components = collect_system->GetVisibleComponents();
            LogDebug("[LineRenderSystem::SyncComponentsToRenderer] LineCollectSystem: visible_components=%zu dirty_count=%u",
                     components.size(), collect_system->GetVisibleDirtyCount());
        }
        else
        {
            LogDebug("[LineRenderSystem::SyncComponentsToRenderer] No LineCollectSystem, falling back to all LinesComponent");
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

            // Skip only if: already uploaded AND not dirty AND same component
            if (has_uploaded_once && !comp->dirty && active_component_keys.find(key) != active_component_keys.end())
            {
                LogDebug("[LineRenderSystem::SyncComponentsToRenderer] SKIP component (already synced, not dirty)");
                uploaded_line_count += static_cast<uint32_t>(comp->lines.size());
                continue;
            }

            LogDebug("[LineRenderSystem::SyncComponentsToRenderer] Uploading component key=%llu width=%u lines=%zu",
                     key, comp->width, comp->lines.size());
            
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
                {
                    LogDebug("[LineRenderSystem::SyncComponentsToRenderer] Removing old component key=%llu", key);
                    line_manager->RemoveComponentLines(key);
                }
            }
        }

        LogDebug("[LineRenderSystem::SyncComponentsToRenderer] CommitComponentLines, uploaded=%u", uploaded_line_count);
        line_manager->CommitComponentLines();

        active_component_keys.swap(new_active_keys);
        last_uploaded_line_count = uploaded_line_count;
        
        // Mark as uploaded after first successful sync
        if (!has_uploaded_once)
        {
            has_uploaded_once = true;
            LogDebug("[LineRenderSystem::SyncComponentsToRenderer] First upload complete, has_uploaded_once=true");
        }

        if (collect_system)
            last_collect_visible_set_signature = collect_system->GetVisibleSetSignature();
        
        LogDebug("[LineRenderSystem::SyncComponentsToRenderer] COMPLETE uploaded=%u", uploaded_line_count);
    }

    void LineRenderSystem::Render(graph::RenderCmdBuffer *cmd, float /*deltaTime*/)
    {
        LogDebug("[LineRenderSystem::Render] START (RenderDebug phase for stats/debug) cmd=%p line_manager=%p", cmd, line_manager);
        
        if (!cmd)
        {
            LogError("[LineRenderSystem::Render] cmd is null");
            return;
        }

        if (!line_manager)
        {
            LogDebug("[LineRenderSystem::Render] line_manager is null, skipping draw");
            return;
        }

        // At this point, we're in RenderDebug phase.
        // RenderBatch (Update) has already synced data to LineRenderManager.
        // RenderBufferCommit/RenderBufferUpload have already uploaded geometry VAB.
        // Just submit the draw commands.
        
        LogDebug("[LineRenderSystem::Render] Calling line_manager->Draw()");
        line_manager->Draw(cmd);
        LogDebug("[LineRenderSystem::Render] COMPLETE");
    }
}//namespace hgl::ecs
