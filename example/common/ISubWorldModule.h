#pragma once

#include<hgl/ecs/components/SubWorldComponent.h>
#include<hgl/ecs/core/Context.h>

#include<string>

namespace hgl::graph
{
    class RenderContext;
}

namespace example::modules
{
    class ISubWorldModule
    {
    public:
        virtual ~ISubWorldModule() = default;

        virtual bool Mount(hgl::graph::RenderContext* render_context,
                           hgl::ecs::ECSContext* root_context,
                           const std::string& anchor_name,
                           hgl::ecs::SubWorldMode mode) = 0;
    };
}
