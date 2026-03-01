#pragma once

#include<hgl/ecs/core/System.h>
#include<hgl/vk/StructuredBufferAccessor.h>
#include<hgl/graph/env/SkyInfo.h>
#include<hgl/graph/mtl/SkyLight.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>

namespace hgl
{
    namespace graph
    {
        class RenderContext;
    }

    namespace ecs
    {
        using UBOSkyInfo = graph::StructuredBufferAccessor<graph::SkyInfo>;

        /// SkyLight 运行时资源需求（由 EnvironmentSystem 统一声明，渲染/材质侧只消费结果）
        struct SkyLightResourceRequirement
        {
            bool need_cubemap = false;     ///<是否需要天空/环境 CubeMap
            bool need_ibl_cubemap = false; ///<是否需要 IBL CubeMap
            bool need_sh_ubo = false;      ///<是否需要 SH 系数 UBO
        };

        /// SkyLight 资源绑定占位（仅接口，不在本阶段实现真实资源分配）
        struct SkyLightResourceBinding
        {
            const char *cubemap_name = nullptr;      ///<CubeMap 资源标识（资源系统 key/path/name）
            const char *ibl_cubemap_name = nullptr;  ///<IBL CubeMap 资源标识
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

            // SkyLight 配置与占位绑定（接口阶段）
            graph::mtl::SkyLightAmbientModel skylight_model = graph::mtl::SkyLightAmbientModel::Simple;
            SkyLightResourceBinding skylight_binding;

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
            // SkyLight 配置接口（阶段 1：声明与最小联动；资源创建/同步留待后续实现）
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
                out.need_ibl_cubemap = req.need_ibl_cubemap;
                return out;
            }

            /// 占位接口：设置当前模型对应的资源绑定（不触发真实加载）
            void SetSkyLightResourceBinding(const SkyLightResourceBinding &binding)
            {
                skylight_binding = binding;
                SyncSkyLightBindingKeysFromRequirement();
            }

            /// 占位接口：获取当前资源绑定
            const SkyLightResourceBinding &GetSkyLightResourceBinding() const { return skylight_binding; }

            /// 便捷接口：设置单 CubeMap 资源名（当前阶段最常用）
            void SetSkyCubeMapName(const char *name) { skylight_binding.cubemap_name = name; }

            /// 便捷接口：获取单 CubeMap 资源名
            const char *GetSkyCubeMapName() const { return skylight_binding.cubemap_name; }

            /// 占位接口：检查当前绑定是否满足模型需求（仅字符串非空检查）
            bool IsSkyLightResourceReady() const
            {
                const graph::mtl::SkyLightDataRequirement req =
                    graph::mtl::GetSkyLightDataRequirement(skylight_model);

                if (req.need_sky_cubemap && (!skylight_binding.cubemap_name || !*skylight_binding.cubemap_name))
                    return false;
                if (req.need_ibl_cubemap && (!skylight_binding.ibl_cubemap_name || !*skylight_binding.ibl_cubemap_name))
                    return false;
                if (req.need_sh_ubo && (!skylight_binding.sh_ubo_name || !*skylight_binding.sh_ubo_name))
                    return false;

                return true;
            }

            /// 对接材质创建配置：把 EnvironmentSystem 的天光策略投影到 Material3DCreateConfig
            /// 说明：本接口只负责配置传递，不直接创建/绑定 GPU 资源。
            void ApplySkyLightToMaterialConfig(graph::mtl::Material3DCreateConfig &cfg) const
            {
                cfg.sky = true;
                cfg.sky_ambient_model = skylight_model;
            }

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

                if (req.need_ibl_cubemap && (!skylight_binding.ibl_cubemap_name || !*skylight_binding.ibl_cubemap_name) && req.ibl_cubemap_name)
                    skylight_binding.ibl_cubemap_name = req.ibl_cubemap_name;
            }

            void EnsureResources();
        };
    }//namespace ecs
}//namespace hgl

