#pragma once

#include <hgl/graph/geo/line/LineRenderManager.h>
#include <memory>

namespace hgl::ecs
{
    class ECSContext;
    class LineRenderSystem;
}

namespace hgl::graph
{
    class LineRenderService
    {
    private:
        LineRenderManager *line_manager = nullptr;
        RenderContext *render_context = nullptr;
        IRenderTarget *render_target = nullptr;

    public:
        LineRenderService() = default;
        ~LineRenderService();

        bool Init(hgl::ecs::ECSContext *ecs);
        void SetRenderContext(RenderContext *ctx);
        void SetRenderTarget(IRenderTarget *rt);

        void SetColor(int index, const Color4f &c);
        bool AddLine(const Vector3f &from, const Vector3f &to, uint8 color_index, uint8 width = 1);
        bool AddLines(uint8 width, const std::vector<LineSegmentDescriptor> &list);
        void ClearLines();
        void Draw(RenderCmdBuffer *cmd);

        LineRenderManager *GetManager() const { return line_manager; }

    private:
        void EnsureManager();
    };

    std::shared_ptr<hgl::ecs::LineRenderSystem> RegisterLineRenderService(hgl::ecs::ECSContext *ecs, LineRenderService *service);
}
