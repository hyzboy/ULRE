#pragma once

#include<hgl/ecs/core/System.h>
#include<hgl/mtl/ShaderResourceSchema.h>
#include<hgl/type/UnorderedMap.h>
#include<hgl/type/String.h>
#include<cstdint>
#include<vector>
#include<unordered_map>
#include<unordered_set>

namespace hgl::graph {
    template<typename> class StructuredBufferAccessor;
    struct ViewportInfo;
    class DeviceBuffer;
}

#ifndef ULRE_ECS_DEBUG_API
#define ULRE_ECS_DEBUG_API 1
#endif

namespace hgl::graph
{
    class RenderCmdBuffer;
    class ShaderProgram;
    class IRenderTarget;
    class IGPUBuffer;
    class Texture;
    class Sampler;
    class BindlessTextureManager;
}

namespace hgl::ecs
{
    class RenderItem;

    /**
     * RenderDescriptorBindingSystem
     *
     * Centralized descriptor-binding submission point in ECS render pipeline.
     * It gathers descriptor bindings from render target and camera-related systems
     * and pushes them into current RenderCmdBuffer at RenderFrameSync phase.
     */
    class RenderDescriptorBindingSystem : public System
    {
    private:

        struct ResourceLayoutDiagStats
        {
            uint32_t materials_checked = 0;
            uint32_t materials_unresolved = 0;
            uint32_t required_missing = 0;
            uint32_t optional_missing = 0;
            uint32_t fallback_hits = 0;

            bool operator==(const ResourceLayoutDiagStats &rhs) const
            {
                return materials_checked == rhs.materials_checked
                    && materials_unresolved == rhs.materials_unresolved
                    && required_missing == rhs.required_missing
                    && optional_missing == rhs.optional_missing
                    && fallback_hits == rhs.fallback_hits;
            }

            bool operator!=(const ResourceLayoutDiagStats &rhs) const
            {
                return !(*this == rhs);
            }
        };

        // Viewport UBO — owned here, stable across swapchain resize.
        graph::StructuredBufferAccessor<graph::ViewportInfo> *viewport_ubo = nullptr;
        uint32_t pending_viewport_width  = 0;
        uint32_t pending_viewport_height = 0;
        std::unordered_map<const graph::ShaderProgram *, bool> resource_layout_last_ok;
        bool resource_layout_diagnostics_enabled = true;
        ResourceLayoutDiagStats last_contract_stats{};
        std::unordered_set<graph::ShaderProgram *> pipeline_materials;
        // resource_id → bindless descriptor index (1-based, 0 = not found).
        // Filled by RegisterTexture2D(Array)Resource; consumed by
        // RenderPrimitiveCollectSystem::MaterializeRecipeRowsForPrimitive to
        // build per-primitive texture layer rows without a shared spec/cache.
        hgl::UnorderedMap<AnsiString, uint32_t> materialization_resource_handles;

    public:
        RenderDescriptorBindingSystem(const std::string& name = "RenderDescriptorBindingSystem");
        ~RenderDescriptorBindingSystem() override;

        graph::ViewportInfo *GetViewportInfo();
        void SetViewportExtent(uint32_t w, uint32_t h);

        void Update(float deltaTime) override;
        void Render(graph::RenderCmdBuffer *cmd, float deltaTime) override;

        bool GetContractDiagnosticsStats(uint32_t &materials_checked,
                                        uint32_t &materials_unresolved,
                                        uint32_t &required_missing,
                                        uint32_t &optional_missing,
                                        uint32_t &fallback_hits) const;
        bool GetMaterialBindingRegistryStats(uint32_t &materials_registered,
                             uint32_t &binding_entries) const;
        void RegisterPipelineMaterial(graph::ShaderProgram *material);
        void UnregisterPipelineMaterial(graph::ShaderProgram *material);
        bool RegisterMaterialStructLayout(graph::mtl::SSBOType ssbo_type,
                                          uint32_t ssbo_id,
                                          uint32_t byte_stride);

        /**
         * 便利版本：向 BindlessTextureManager 注册纹理并预注册 pool 映射。
         * 所有纹理（2D / 2DArray）统一注册到 sampler2DArray[]（binding=0）；2D 走单层 array view。
         * sampler 已由统一 Sampler 注册机制（binding=1）按名字提供，此处只注册纹理本身。
         * @return 1-based tex_handle，失败返回 0
         */
        uint32_t RegisterTextureResource(const std::string &resource_id,
                                         graph::Texture *tex,
                                         graph::BindlessTextureManager *bindless_mgr);

        /**
         * 查询 resource_id 对应的 bindless descriptor index（1-based）。
         * 注册期间 RegisterTextureResource 已把 handle 存入映射；
         * 未注册/不存在返回 0。MaterializeRecipeRowsForPrimitive 用它
         * 把纹理 handle 写入引擎管理的纹理行表域 SSBO。
         */
        uint32_t GetBindlessHandle(const AnsiString &resource_id) const;

    private:

        void EnsureViewportUBO();
        void ReleaseViewportUBO();
        void SyncBindingsForCurrentCommand(graph::RenderCmdBuffer *cmd, bool run_contract_diagnostics);
        void ApplyResourceLayoutBindings(graph::RenderCmdBuffer *cmd);
        const graph::IGPUBuffer *ResolveViewportUBO() const;
        const graph::IGPUBuffer *ResolveCameraUBO() const;
        const graph::IGPUBuffer *ResolveSkyUBO();
        void ValidateResourceLayoutsSideChannel();
        bool IsSemanticResolvable(graph::mtl::DescriptorSemantic semantic) const;
    };
}
