#pragma once

#include <hgl/ecs/core/system/System.h>

namespace hgl::ecs
{
    class ECSContext;

    /**
     * RenderSystem
     *
     * 渲染世界侧的系统基类：在纯核心 System 之上补充 ECSContext* 渲染世界指针，
     * 供渲染期/流送/调试/管线系统访问世界级渲染资源与环境。
     */
    class RenderSystem : public System
    {
    protected:
        ECSContext *context = nullptr;

    public:
        explicit RenderSystem(const char *name) : System(name) {}
        explicit RenderSystem(const AnsiString &name = "RenderSystem") : System(name) {}
        explicit RenderSystem(const std::string &name) : System(name) {}
        ~RenderSystem() override = default;

        ECSContext *GetContext() const { return context; }
        void SetContext(ECSContext *ctx) { context = ctx; }
    };
}
