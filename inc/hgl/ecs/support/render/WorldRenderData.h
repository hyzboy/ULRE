#pragma once

#include <hgl/type/Smart.h>
#include <hgl/type/String.h>
#include <hgl/ecs/support/storage/LightRowTable.h>
#include <hgl/graph/ubo/ShadowInfo.h>
#include <hgl/ecs/support/material/MaterialVariantTable.h>
#include <hgl/ecs/support/material/MaterialRuntimeTable.h>
#include <hgl/ecs/support/material/MaterialAuthoringSideTable.h>
#include <hgl/ecs/support/material/MaterialRuntimePipelineTable.h>
#include <hgl/ecs/support/material/ModelTemplateMaterialTable.h>
#include <hgl/ecs/support/storage/CameraInfoStorage.h>
#include <hgl/ecs/components/CameraComponent.h>

namespace hgl::graph
{
    class VulkanDevice;
    class GraphicsContext;
    class IRenderTarget;
    class TextureManager;
}

namespace hgl::ecs
{
    class WorldRenderEnvironment;
    class WorldRenderRunner;

    /**
     * WorldRenderData
     *
     * 聚合世界私有的 CPU 渲染数据表与元数据：
     * - 光源行表 (LightRowTable)
     * - 阴影参数 (ShadowInfo)
     * - 材质变体表 (MaterialVariantTable)
     * - 材质运行期表 (MaterialRuntimeTable)
     * - 材质重类型授权数据旁表 (MaterialAuthoringSideTable)
     * - 运行期管线缓存表 (MaterialRuntimePipelineTable)
     * - 模板材质表 (ModelTemplateMaterialTable)
     * - 世界相机行存储 (CameraInfoStorage) 与常驻 Fallback 相机
     */
    class WorldRenderData
    {
    private:
        WorldRenderEnvironment *environment = nullptr;
        WorldRenderRunner *render_runner = nullptr;
        AnsiString world_name;
        const void *world_identity = nullptr;

        /// 光源行表（LightRow，世界私有；行 0 = 主方向光）
        LightRowTable light_row_table;

        /// 影子数据的 CPU 权威（世界私有）
        graph::ShadowInfo shadow_info;

        /// 材质变体表
        AutoDelete<MaterialVariantTable> material_variant_table;

        /// 材质运行期表
        AutoDelete<MaterialRuntimeTable> material_runtime_table;

        /// 材质重类型授权数据旁表
        AutoDelete<MaterialAuthoringSideTable> material_authoring_side_table;

        /// 运行期管线缓存表
        AutoDelete<MaterialRuntimePipelineTable> material_runtime_pipeline_table;

        /// 模板材质表
        AutoDelete<ModelTemplateMaterialTable> model_template_material_table;

        /// 相机表（世界级资源）
        std::shared_ptr<CameraInfoStorage> camera_info_storage;

        /// 常驻 fallback 相机
        std::shared_ptr<CameraComponent> fallback_camera;

    public:
        explicit WorldRenderData(WorldRenderEnvironment *environment = nullptr,
                                 WorldRenderRunner *render_runner = nullptr,
                                 const AnsiString &world_name = "World",
                                 const void *world_identity = nullptr);
        ~WorldRenderData();

        WorldRenderData(const WorldRenderData &) = delete;
        WorldRenderData &operator=(const WorldRenderData &) = delete;

        bool InitializeDeviceResources(graph::VulkanDevice *device, const AnsiString &world_name);

        void Clear();
        void DestroyEntityData(EntityID id);

        // ── 光源行表 ──
        LightRowTable &Lights() { return light_row_table; }
        const LightRowTable &Lights() const { return light_row_table; }

        // ── 阴影信息 ──
        graph::ShadowInfo *EditShadow() { return &shadow_info; }
        graph::ShadowInfo &Shadow() { return shadow_info; }
        const graph::ShadowInfo &Shadow() const { return shadow_info; }
        void SetShadow(const graph::ShadowInfo &info) { shadow_info = info; }

        // ── 材质表访问器 ──
        MaterialVariantTable *Variants() { return material_variant_table; }
        const MaterialVariantTable *Variants() const { return material_variant_table; }

        MaterialRuntimePipelineTable &GetMaterialRuntimePipelineTable() { return *material_runtime_pipeline_table; }
        const MaterialRuntimePipelineTable &GetMaterialRuntimePipelineTable() const { return *material_runtime_pipeline_table; }

        MaterialAuthoringSideTable *AuthoringMaterials() { return material_authoring_side_table; }
        const MaterialAuthoringSideTable *AuthoringMaterials() const { return material_authoring_side_table; }

        MaterialRuntimeTable *RuntimeMaterials() { return material_runtime_table; }
        const MaterialRuntimeTable *RuntimeMaterials() const { return material_runtime_table; }

        ModelTemplateMaterialTable *TemplateMaterials() { return model_template_material_table; }
        const ModelTemplateMaterialTable *TemplateMaterials() const { return model_template_material_table; }

        // ── 材质数据及运行期槽 ──
        MaterialRuntimeSlot &GetOrCreateMaterialRuntimeSlot(EntityID owner);
        MaterialRuntimeSlot *GetMaterialRuntimeSlot(EntityID owner);

        MaterialData *GetMaterialData(EntityID owner);
        const MaterialData *GetMaterialData(EntityID owner) const;
        MaterialData *GetMaterialData(Entity *owner);
        const MaterialData *GetMaterialData(const Entity *owner) const;
        MaterialData *GetOrCreateMaterialData(EntityID owner);
        MaterialData *GetOrCreateMaterialData(Entity *owner);

        void InvalidateEntityRuntimePipeline(EntityID owner);
        void NotifyPipelineInvalidated() const;
        void NotifyModelTemplateChanged() const;

        graph::GraphicsContext *GetGraphicsContext();
        graph::IRenderTarget *GetRenderTarget();
        graph::TextureManager *GetTextureManager();
        uint64_t GetRenderSubmissionSerial() const;
        bool IsGPUQuiescent() const;
        const void *GetWorldIdentity() const { return world_identity; }

        // ── 相机管理 ──
        CameraInfoStorage *CameraRows() { return camera_info_storage.get(); }
        const CameraInfoStorage *CameraRows() const { return camera_info_storage.get(); }
        std::weak_ptr<CameraInfoStorage> GetCameraInfoStorageWeak() const { return camera_info_storage; }

        CameraComponent *GetCamera(uint32_t slot) const;
        CameraComponent *GetDefaultCamera() const;
        bool SetDefaultCamera(const std::shared_ptr<CameraComponent> &camera);
        uint32_t GetCameraSlot(const CameraComponent *camera) const;

        bool EnsureCameraSlot(CameraComponent *camera, bool is_default = false);
        bool ReleaseCameraSlot(const CameraComponent *camera);

        CameraComponent *EnsureFallbackCamera();
        bool IsFallbackCamera(const CameraComponent *camera) const;
        CameraComponent *GetFallbackCamera() const { return fallback_camera.get(); }
    };
}
