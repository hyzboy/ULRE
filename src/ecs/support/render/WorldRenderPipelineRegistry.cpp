#include <hgl/ecs/support/render/WorldRenderPipelineRegistry.h>
#include <hgl/ecs/support/pipeline/RenderPipelineBase.h>

namespace hgl::ecs
{
    WorldRenderPipelineRegistry::~WorldRenderPipelineRegistry()
    {
        Clear();
    }

    RenderPipelineBase *WorldRenderPipelineRegistry::Get(const char *name)
    {
        if (!name)
            return nullptr;

        return Get(AnsiString(name));
    }

    RenderPipelineBase *WorldRenderPipelineRegistry::Get(const AnsiString &name)
    {
        RenderPipelineBase **pipeline = pipelines.GetValuePointer(name);
        return pipeline ? *pipeline : nullptr;
    }

    void WorldRenderPipelineRegistry::Register(const char *name, RenderPipelineBase *pipeline)
    {
        if (!name)
            return;

        Register(AnsiString(name), pipeline);
    }

    void WorldRenderPipelineRegistry::Register(const AnsiString &name, RenderPipelineBase *pipeline)
    {
        if (!pipeline)
            return;

        RenderPipelineBase **old = pipelines.GetValuePointer(name);
        if (old && *old)
            delete *old;

        pipelines[name] = pipeline;
    }

    void WorldRenderPipelineRegistry::Clear()
    {
        for (auto &pair : pipelines)
            delete pair.second;

        pipelines.Clear();
    }
}
