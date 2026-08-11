#pragma once

#include<hgl/ecs/core/System.h>
#include<hgl/mtl/ShaderResourceSchema.h>
#include<hgl/mtl/MaterializationPools.h>
#include<hgl/type/String.h>
#include<limits>
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

        struct MaterialResourceBinding
        {
            graph::Texture *texture = nullptr;
            graph::Sampler *sampler = nullptr;
        };

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

        struct MaterializationResolveCacheEntry
        {
            graph::mtl::MaterializationSharedSpec shared_spec;
        };

        // Viewport UBO — owned here, stable across swapchain resize.
        graph::StructuredBufferAccessor<graph::ViewportInfo> *viewport_ubo = nullptr;
        uint32_t pending_viewport_width  = 0;
        uint32_t pending_viewport_height = 0;
        std::unordered_map<const graph::ShaderProgram *, bool> resource_layout_last_ok;
        std::unordered_map<const graph::ShaderProgram *, std::unordered_map<std::string, MaterialResourceBinding>> material_resource_bindings;
        bool resource_layout_diagnostics_enabled = true;
        ResourceLayoutDiagStats last_contract_stats{};
        std::unordered_set<graph::ShaderProgram *> pipeline_materials;
        graph::mtl::BindlessTexturePool materialization_texture_pool;
        graph::mtl::StructDataPool materialization_struct_pool;
        graph::mtl::MaterializationIndexTables materialization_index_tables;
        graph::mtl::MaterializationResolveCallbacks materialization_callbacks;
        std::unordered_map<uint64_t, MaterializationResolveCacheEntry> materialization_resolve_cache;
        uint32_t materialization_last_reset_frame = std::numeric_limits<uint32_t>::max();
        graph::DeviceBuffer *materialization_texture_layer_ssbo = nullptr;
        graph::DeviceBuffer *materialization_data_index_table_buffer = nullptr;
        uint32_t materialization_texture_layer_capacity = 0;
        uint32_t materialization_data_index_table_capacity = 0;
        uint32_t materialization_data_slot_count = 0;  // max slot count across all active rows
        bool materialization_index_tables_dirty = false;

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
        bool GetMaterialBindingKeys(const graph::ShaderProgram *material,
                        std::vector<std::string> &out_keys) const;
        bool RegisterMaterialTexture(graph::ShaderProgram *material,
                         const AnsiString &name,
                         graph::Texture *texture);
        bool RegisterMaterialTextureSampler(graph::ShaderProgram *material,
                            const AnsiString &name,
                            graph::Texture *texture,
                            graph::Sampler *sampler);
        void RemoveMaterialBinding(graph::ShaderProgram *material, const AnsiString &name);
        void ClearMaterialBindings(graph::ShaderProgram *material);
        void RegisterPipelineMaterial(graph::ShaderProgram *material);
        void UnregisterPipelineMaterial(graph::ShaderProgram *material);
        bool RegisterMaterialStructLayout(graph::mtl::SSBOType ssbo_type,
                                          uint32_t ssbo_id,
                                          uint32_t byte_stride);
        void ResetMaterializationFrameData();
        bool WriteTextureLayerRowAt(uint32_t at_index, const graph::mtl::MaterializationSpec &spec);
        bool ResolveMaterialRecipe(const graph::mtl::MaterialRecipe &recipe,
                                   graph::mtl::MaterializationSpec &out_spec,
                                   uint32_t *out_texture_layer_row = nullptr,
                                   uint32_t *out_data_index_row = nullptr,
                                   graph::mtl::MaterializationInstanceData *out_instance_data = nullptr);
        bool GetMaterializationPoolStats(uint32_t &texture_count,
                                         uint32_t &struct_layout_count,
                                         uint32_t &texture_layer_rows,
                                         uint32_t &data_index_rows) const;

        /**
         * 向 bindless 纹理池预注册一个逻辑资源 ID 与 bindless handle 的映射。
         * 必须在调用 ResolveMaterialRecipe 之前调用，以确保 recipe 中的 resource_id
         * 能被正确解析为 bindless handle。
         *
         * 典型用法：
         *   uint32_t handle = bindless_mgr->Register2D(tex, sampler);
         *   rdbs->RegisterBindlessTextureResource("Albedo", handle);
         *   // 然后 recipe.textures[i].resource_id = "Albedo"
         */
        bool RegisterBindlessTextureResource(const std::string &resource_id, uint32_t bindless_handle);

        /**
         * 便利版本：同时向 BindlessTextureManager 注册 Vulkan 侧描述符并预注册 pool 映射。
         * @return 1-based handle，失败返回 0
         */
        uint32_t RegisterTexture2DResource(const std::string &resource_id,
                                            graph::Texture *tex,
                                            graph::Sampler *sampler,
                                            graph::BindlessTextureManager *bindless_mgr);

        /**
         * RegisterTexture2DResource 的 2DArray 对称版本。
         * 用于图标集、地形 TILE、植被、NPC 部件等统一格式资产池。
         * 内部调用 bindless_mgr->Register2DArray，handle 写入 bindless_tex2darray[]（binding=1）。
         * @return 1-based handle，失败返回 0
         */
        uint32_t RegisterTexture2DArrayResource(const std::string &resource_id,
                                                 graph::Texture *tex,
                                                 graph::Sampler *sampler,
                                                 graph::BindlessTextureManager *bindless_mgr);

    private:

        void EnsureViewportUBO();
        void ReleaseViewportUBO();
        void SyncBindingsForCurrentCommand(graph::RenderCmdBuffer *cmd, bool run_contract_diagnostics);
        void ApplyResourceLayoutBindings(graph::RenderCmdBuffer *cmd);
        const graph::IGPUBuffer *ResolveViewportUBO() const;
        const graph::IGPUBuffer *ResolveCameraUBO() const;
        const graph::IGPUBuffer *ResolveSkyUBO();
        void EnsureMaterializationCallbacks();
        void ReleaseMaterializationIndexBuffers();
        void UploadMaterializationIndexTables();
        const MaterialResourceBinding *FindMaterialResourceBinding(const graph::ShaderProgram *material, const char *name) const;
        void ValidateResourceLayoutsSideChannel();
        bool IsSemanticResolvable(graph::mtl::DescriptorSemantic semantic) const;
    };
}
