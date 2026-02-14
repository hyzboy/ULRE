#pragma once

#include<hgl/ecs/core/System.h>
#include<hgl/vk/StructuredBufferAccessor.h>
#include<hgl/graph/env/SkyInfo.h>

namespace hgl
{
    namespace graph
    {
        class RenderFramework;
    }

    namespace ecs
    {
        using UBOSkyInfo = graph::StructuredBufferAccessor<graph::SkyInfo>;

        /**
         * EnvironmentSystem
         *
         * Manages global environment data (sky, sun, atmosphere, etc.).
         * Currently owns SkyInfo UBO.
         */
        class EnvironmentSystem : public System
        {
        private:

            graph::RenderFramework *render_framework = nullptr;
            UBOSkyInfo *sky_ubo = nullptr;

        public:

            EnvironmentSystem(const std::string &name = "EnvironmentSystem");
            ~EnvironmentSystem() override;

            void SetRenderFramework(graph::RenderFramework *rf);
            UBOSkyInfo *GetSkyUBO() const { return sky_ubo; }

            graph::SkyInfo *EditSkyInfo();
            const graph::SkyInfo *GetSkyInfo() const;

            void SetSkyInfo(const graph::SkyInfo &info, bool immediate = true);
            void MarkSkyDirty();
            void SyncSkyUBO();

            void Update(float deltaTime) override;

        private:

            void EnsureResources();
        };
    }//namespace ecs
}//namespace hgl

