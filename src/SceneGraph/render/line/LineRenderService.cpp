#include <hgl/graph/geo/line/LineRenderService.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/systems/render/LineRenderSystem.h>
#include <hgl/graph/render/RenderContext.h>
#include <hgl/vk/VKRenderTarget.h>

namespace hgl::graph
{
    LineRenderService::~LineRenderService()
    {
        delete line_manager;
    }

    bool LineRenderService::Init(hgl::ecs::ECSContext *ecs)
    {
        if (!ecs)
            return false;

        render_context = ecs->GetRenderContext();
        render_target = ecs->GetRenderTarget();

        if (!render_context || !render_target)
            return false;

        EnsureManager();
        return line_manager != nullptr;
    }

    void LineRenderService::SetRenderContext(RenderContext *ctx)
    {
        render_context = ctx;
        EnsureManager();
    }

    void LineRenderService::SetRenderTarget(IRenderTarget *rt)
    {
        if (render_target == rt)
            return;

        render_target = rt;
        if (line_manager)
            line_manager->SetRenderTarget(rt);
        else
            EnsureManager();
    }

    void LineRenderService::SetColor(int index, const Color4f &c)
    {
        if (line_manager)
            line_manager->SetColor(index, c);
    }

    bool LineRenderService::AddLine(const Vector3f &from, const Vector3f &to, uint8 color_index, uint8 width)
    {
        if (!line_manager)
            return false;

        return line_manager->AddLine(from, to, color_index, width);
    }

    bool LineRenderService::AddLines(uint8 width, const std::vector<LineSegmentDescriptor> &list)
    {
        if (!line_manager)
            return false;

        return line_manager->AddLine(width, list);
    }

    void LineRenderService::ClearLines()
    {
        if (line_manager)
            line_manager->ClearLines();
    }

    void LineRenderService::Draw(RenderCmdBuffer *cmd)
    {
        if (!cmd || !line_manager)
            return;

        line_manager->Draw(cmd);
    }

    void LineRenderService::EnsureManager()
    {
        if (line_manager || !render_context || !render_target)
            return;

        line_manager = CreateLineRenderManager(render_context, render_target);
    }

    std::shared_ptr<hgl::ecs::LineRenderSystem> RegisterLineRenderService(hgl::ecs::ECSContext *ecs, LineRenderService *service)
    {
        if (!ecs || !service)
            return nullptr;

        auto system = ecs->GetSystem<hgl::ecs::LineRenderSystem>();
        if (!system)
            system = ecs->RegisterRenderSystem<hgl::ecs::LineRenderSystem>();

        if (system)
            system->SetLineRenderService(service);

        return system;
    }
}
