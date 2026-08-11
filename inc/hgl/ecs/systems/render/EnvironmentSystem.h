#pragma once

#include<hgl/ecs/core/System.h>
#include<hgl/vk/StructuredBufferAccessor.h>
#include<hgl/graph/ubo/SkyInfo.h>
#include<hgl/mtl/SkyLight.h>
#include<hgl/mtl/MaterialDefinitionRegistry.h>

namespace hgl
{
    namespace graph
    {
        class RenderContext;
        class Texture;
        class Sampler;
    }

    namespace ecs
    {
        using UBOSkyInfo = graph::StructuredBufferAccessor<graph::SkyInfo>;

        /// SkyLight 运行时资源需求（由 EnvironmentSystem 统一声明，渲染/材质侧只消费结果）
        struct SkyLightResourceRequirement
        {
            bool need_cubemap = false;     ///<是否需要天空/环境 CubeMap
            bool need_sh_ubo = false;      ///<是否需要 SH 系数 UBO
        };

        /// SkyLight resource names supplied by the environment/resource layer.
        struct SkyLightResourceBinding
        {
            const char *cubemap_name = nullptr;      ///<CubeMap 资源标识（资源系统 key/path/name）
            const char *sh_ubo_name = nullptr;       ///<SH UBO 资源标识
        };

        /**
         * EnvironmentSystem
         *
         * Manages global environment data (sky, sun, atmosphere, etc.).
         * Currently owns SkyInfo UBO.
         */
        class EnvironmentSystem : public System
        {
        private:

            graph::RenderContext *render_context = nullptr;
            UBOSkyInfo *sky_ubo = nullptr;
            bool sky_ubo_managed = false;

            // SkyLight configuration and runtime resource bindings.
            graph::mtl::SkyLightAmbientModel skylight_model = graph::mtl::SkyLightAmbientModel::Simple;
            SkyLightResourceBinding skylight_binding;
            graph::Texture *sky_cubemap_texture = nullptr;
            graph::Sampler *sky_cubemap_sampler = nullptr;

        public:

            EnvironmentSystem(const std::string &name = "EnvironmentSystem");
            ~EnvironmentSystem() override;

            void SetRenderContext(graph::RenderContext *ctx) { render_context = ctx; }
            UBOSkyInfo *GetSkyUBO() const { return sky_ubo; }

            graph::SkyInfo *EditSkyInfo();
            const graph::SkyInfo *GetSkyInfo() const;

            void SetSkyInfo(const graph::SkyInfo &info, bool immediate = true);
            void MarkSkyDirty();
            void SyncSkyUBO();

            // -----------------------------------------------------------------
            // SkyLight configuration interface. Resource ownership remains
            // with the environment/resource layer.
            // -----------------------------------------------------------------

            /// 设置当前天光模型；immediate=true 时仅标记 Sky UBO dirty（不做重资源）
            void SetSkyLightAmbientModel(graph::mtl::SkyLightAmbientModel model, bool immediate = true)
            {
                skylight_model = model;
                SyncSkyLightBindingKeysFromRequirement();
                if (immediate)
                    MarkSkyDirty();
            }

            /// 获取当前天光模型
            graph::mtl::SkyLightAmbientModel GetSkyLightAmbientModel() const { return skylight_model; }

            /// 按模型计算资源需求（声明性接口）
            SkyLightResourceRequirement GetSkyLightResourceRequirement() const
            {
                const graph::mtl::SkyLightDataRequirement req =
                    graph::mtl::GetSkyLightDataRequirement(skylight_model);

                SkyLightResourceRequirement out;
                out.need_cubemap = req.need_sky_cubemap;
                out.need_sh_ubo = req.need_sh_ubo;
                return out;
            }

            /// Set logical resource names; loading remains external.
            void SetSkyLightResourceBinding(const SkyLightResourceBinding &binding)
            {
                skylight_binding = binding;
                SyncSkyLightBindingKeysFromRequirement();
            }

            /// Get logical resource names.
            const SkyLightResourceBinding &GetSkyLightResourceBinding() const { return skylight_binding; }

            /// 便捷接口：设置单 CubeMap 资源名（当前阶段最常用）
            void SetSkyCubeMapName(const char *name) { skylight_binding.cubemap_name = name; }

            /// 便捷接口：获取单 CubeMap 资源名
            const char *GetSkyCubeMapName() const { return skylight_binding.cubemap_name; }

            void SetSkyCubeMapResource(graph::Texture *texture, graph::Sampler *sampler)
            {
                sky_cubemap_texture = texture;
                sky_cubemap_sampler = sampler;
            }

            graph::Texture *GetSkyCubeMapTexture() const { return sky_cubemap_texture; }
            graph::Sampler *GetSkyCubeMapSampler() const { return sky_cubemap_sampler; }

            /// Check that required logical and runtime resources are available.
            bool IsSkyLightResourceReady() const
            {
                const graph::mtl::SkyLightDataRequirement req =
                    graph::mtl::GetSkyLightDataRequirement(skylight_model);

                if (req.need_sky_cubemap
                 && ((!skylight_binding.cubemap_name || !*skylight_binding.cubemap_name)
                  || !sky_cubemap_texture
                  || !sky_cubemap_sampler))
                    return false;
                if (req.need_sh_ubo && (!skylight_binding.sh_ubo_name || !*skylight_binding.sh_ubo_name))
                    return false;

                return true;
            }

            /// 对接内部材质构建请求：把 EnvironmentSystem 的天光策略投影到 MaterialDefinitionBuildRequest。
            /// 说明：本接口只负责配置传递，不直接创建/绑定 GPU 资源。
            void ApplySkyLightToMaterialBuildRequest(graph::mtl::MaterialDefinitionBuildRequest &request) const
            {
                request.override_sky_ambient_model = true;
                request.sky_ambient_model = skylight_model;
            }

            void Initialize() override;
            void Update(float deltaTime) override;

        private:

            void SyncSkyLightBindingKeysFromRequirement()
            {
                const graph::mtl::SkyLightDataRequirement req =
                    graph::mtl::GetSkyLightDataRequirement(skylight_model);

                if (req.need_sky_cubemap && (!skylight_binding.cubemap_name || !*skylight_binding.cubemap_name) && req.sky_cubemap_name)
                    skylight_binding.cubemap_name = req.sky_cubemap_name;

                if (req.need_sh_ubo && (!skylight_binding.sh_ubo_name || !*skylight_binding.sh_ubo_name) && req.sh_ubo_name)
                    skylight_binding.sh_ubo_name = req.sh_ubo_name;

            }

            void EnsureResources();
        };
    }//namespace ecs
}//namespace hgl
