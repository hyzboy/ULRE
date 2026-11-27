#pragma once

#include <hgl/type/String.h>
#include <hgl/type/UnorderedMap.h>

namespace hgl::ecs
{
    class RenderPipelineBase;

    class WorldRenderPipelineRegistry
    {
    private:
        hgl::UnorderedMap<AnsiString, RenderPipelineBase*> pipelines;

    public:
        ~WorldRenderPipelineRegistry();

        RenderPipelineBase *Get(const char *name);
        RenderPipelineBase *Get(const AnsiString &name);

        void Register(const char *name, RenderPipelineBase *pipeline);
        void Register(const AnsiString &name, RenderPipelineBase *pipeline);
        void Clear();
    };
}
