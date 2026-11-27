#include <hgl/ecs/support/render/WorldRenderData.h>
#include <hgl/ecs/core/entity/Entity.h>
#include <hgl/log/Log.h>

namespace hgl::ecs
{
    WorldRenderData::WorldRenderData(IWorldRenderDataSource *value, const void *identity)
        : source(value)
        , world_identity(identity)
        , material_variant_table(new MaterialVariantTable())
        , material_runtime_table(new MaterialRuntimeTable())
        , material_authoring_side_table(new MaterialAuthoringSideTable())
        , material_runtime_pipeline_table(new MaterialRuntimePipelineTable())
        , model_template_material_table(new ModelTemplateMaterialTable())
        , camera_info_storage(std::make_shared<CameraInfoStorage>())
    {
    }

    WorldRenderData::~WorldRenderData()
    {
        Clear();
    }

    bool WorldRenderData::InitializeDeviceResources(graph::VulkanDevice *device, const AnsiString &world_name)
    {
        if (camera_info_storage && !camera_info_storage->IsReady())
        {
            if (!camera_info_storage->Create(device, world_name.c_str()))
            {
                GLogError("[WorldRenderData] 世界相机行存储（CameraInfoStorage）创建失败");
                return false;
            }
        }
        return true;
    }

    void WorldRenderData::Clear()
    {
        if (model_template_material_table)
            model_template_material_table->Clear();

        if (camera_info_storage)
            camera_info_storage->Reset();

        fallback_camera = nullptr;
    }

    void WorldRenderData::DestroyEntityData(EntityID id)
    {
        if (material_runtime_table)
            material_runtime_table->DestroySlot(id);

        if (material_runtime_pipeline_table)
            material_runtime_pipeline_table->Clear(id);
        if (material_authoring_side_table)
            material_authoring_side_table->Remove(id);
    }

    MaterialRuntimeSlot &WorldRenderData::GetOrCreateMaterialRuntimeSlot(EntityID owner)
    {
        return material_runtime_table->GetOrCreateSlot(owner);
    }

    MaterialRuntimeSlot *WorldRenderData::GetMaterialRuntimeSlot(EntityID owner)
    {
        return material_runtime_table ? material_runtime_table->GetSlot(owner) : nullptr;
    }

    MaterialData *WorldRenderData::GetMaterialData(EntityID owner)
    {
        return material_authoring_side_table ? material_authoring_side_table->Find(owner) : nullptr;
    }

    const MaterialData *WorldRenderData::GetMaterialData(EntityID owner) const
    {
        const MaterialAuthoringSideTable *table = material_authoring_side_table;
        return table ? table->Find(owner) : nullptr;
    }

    MaterialData *WorldRenderData::GetMaterialData(Entity *owner)
    {
        return owner ? GetMaterialData(owner->GetEntityID()) : nullptr;
    }

    const MaterialData *WorldRenderData::GetMaterialData(const Entity *owner) const
    {
        return owner ? GetMaterialData(owner->GetEntityID()) : nullptr;
    }

    MaterialData *WorldRenderData::GetOrCreateMaterialData(EntityID owner)
    {
        return material_authoring_side_table ? material_authoring_side_table->GetOrCreate(owner) : nullptr;
    }

    MaterialData *WorldRenderData::GetOrCreateMaterialData(Entity *owner)
    {
        return owner ? GetOrCreateMaterialData(owner->GetEntityID()) : nullptr;
    }

    void WorldRenderData::InvalidateEntityRuntimePipeline(EntityID owner)
    {
        if (material_runtime_pipeline_table)
            material_runtime_pipeline_table->Clear(owner);

        if (source)
            source->RenderDataPipelineInvalidated();
    }

    CameraComponent *WorldRenderData::GetCamera(const uint32_t slot) const
    {
        return camera_info_storage ? camera_info_storage->GetSlotOwner(slot) : nullptr;
    }

    CameraComponent *WorldRenderData::GetDefaultCamera() const
    {
        return GetCamera(CameraInfoStorage::kDefaultCameraSlot);
    }

    bool WorldRenderData::SetDefaultCamera(const std::shared_ptr<CameraComponent> &camera)
    {
        if (!camera_info_storage)
            return false;

        return camera_info_storage->BindCameraSlot(CameraInfoStorage::kDefaultCameraSlot, camera);
    }

    uint32_t WorldRenderData::GetCameraSlot(const CameraComponent *camera) const
    {
        if (!camera || !camera_info_storage)
            return CameraComponent::kInvalidSlot;

        return camera_info_storage->FindSlot(camera);
    }

    bool WorldRenderData::EnsureCameraSlot(CameraComponent *camera, const bool is_default)
    {
        if (!camera || !camera_info_storage)
            return false;

        auto owner = std::static_pointer_cast<CameraComponent>(camera->weak_from_this().lock());
        if (!owner)
        {
            GLogError("[WorldRenderData] 相机 \"%s\" 没有 shared_ptr 宿主，无法认领相机槽"
                      "（槽账目按宿主弱引用计占用期）", camera->GetName().c_str());
            return false;
        }

        if (is_default)
        {
            const uint32_t old_slot = camera_info_storage->FindSlot(camera);
            if (old_slot != CameraInfoStorage::INVALID_SLOT
             && old_slot != CameraInfoStorage::kDefaultCameraSlot)
                camera_info_storage->ReleaseCameraSlot(old_slot);

            if (!SetDefaultCamera(owner))
                return false;

            camera->world_owner = world_identity;
            return true;
        }

        if (camera_info_storage->FindSlot(camera) != CameraInfoStorage::INVALID_SLOT)
            return true;

        if (!camera_info_storage->IsReady())
            return false;

        const uint32_t slot = camera_info_storage->AcquireCameraSlot(owner);

        if (slot == CameraInfoStorage::INVALID_SLOT)
        {
            GLogError("[WorldRenderData] 世界相机槽耗尽（上限 %u）：相机 \"%s\" 未拿到槽，"
                      "本帧不会发布它的相机行——检查是否有相机未归还",
                      CameraInfoStorage::kSlotCapacity, camera->GetName().c_str());
            return false;
        }

        camera->world_owner = world_identity;
        return true;
    }

    bool WorldRenderData::ReleaseCameraSlot(const CameraComponent *camera)
    {
        if (!camera_info_storage || !camera)
            return false;

        return camera_info_storage->ReleaseCameraSlot(camera);
    }

    CameraComponent *WorldRenderData::EnsureFallbackCamera()
    {
        if (fallback_camera)
        {
            SetDefaultCamera(fallback_camera);
            return fallback_camera.get();
        }

        auto camera = std::make_shared<CameraComponent>("WorldFallbackCamera");
        camera->position       = math::Vector3f(0.0f, 0.0f, 0.0f);
        camera->target         = math::Vector3f(0.0f, 0.0f, 1.0f);
        camera->is_main_camera = false;
        camera->matrix_dirty   = true;
        camera->world_owner    = world_identity;

        fallback_camera = camera;
        SetDefaultCamera(camera);

        GLogInfo("[ECS] %s: 世界内没有相机 ⇒ 在 (0,0,0) 生成常驻 fallback 相机（占 0 号槽）",
                 source ? source->RenderDataWorldName().c_str() : "World");

        return fallback_camera.get();
    }

    bool WorldRenderData::IsFallbackCamera(const CameraComponent *camera) const
    {
        return camera != nullptr && camera == fallback_camera.get();
    }
}
