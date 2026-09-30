#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/log/Log.h>
#include<hgl/graph/CameraInfo.h>
#include<hgl/graph/ubo/ViewportInfo.h>
#include<algorithm>
#include<cmath>

namespace hgl
{
    namespace ecs
    {
        TransformComponent::TransformComponent(Mobility initial_mobility, const std::string& name)
            : Component(name)
            , storageHandle(TransformDataStorage::INVALID_HANDLE)
            , bound_storage(nullptr)
            , mobility(initial_mobility)
            , fixed_pixel_sizing_enabled(false)
            , fixed_pixel_diameter(160.0f)
            , fixed_pixel_reference_world_diameter(1.0f)
            , fixed_pixel_min_scale(0.01f)
            , fixed_pixel_camera_info(nullptr)
            , fixed_pixel_viewport_info(nullptr)
        {
        }

        TransformComponent::~TransformComponent()
        {
            // Free storage in the corresponding SOA storage
            if (storageHandle != TransformDataStorage::INVALID_HANDLE && bound_storage)
            {
                bound_storage->Deallocate(storageHandle);
            }
        }

        TransformDataStorage::HandleID TransformComponent::GetStorageHandle() const
        {
            if (storageHandle == TransformDataStorage::INVALID_HANDLE)
            {
                const_cast<TransformComponent*>(this)->EnsureStorageAllocated();
            }
            return storageHandle;
        }

        void TransformComponent::EnsureStorageAllocated()
        {
            if (storageHandle != TransformDataStorage::INVALID_HANDLE)
                return;

            auto* storage = GetStorage();
            if (storage)
            {
                bound_storage = storage;
                storageHandle = storage->Allocate();      // 新行默认就是单位 TRS，无需再写一遍
                storage->SetOwner(storageHandle, owner_id);   // 实体→行 的反向索引（accessor 靠它找变换）
                storage->SetMobility(storageHandle, IsMovable() ? 1 : 0);
            }
        }

        glm::vec3 TransformComponent::GetLocalPosition() const
        {
            return GetAccessor().GetLocalPosition();
        }

        void TransformComponent::SetLocalPosition(const glm::vec3& pos)
        {
            // D4：运行期写 Static 物体 → 一次性告警（构造期写入不计）
            WarnStaticRuntimeWrite("SetLocalPosition");
            GetAccessor().SetLocalPosition(pos);
            MarkDirty(ToChangeMask(TransformChange::Position));

        #if HGL_TRANSFORM_DEBUG_LOGGING
            static uint32_t s_pos_log_tick = 0;
            ++s_pos_log_tick;
            if ((s_pos_log_tick % 120u) == 1u)
            {
                GLogInfo("[TransformComponent] SetLocalPosition: owner=%u handle=%u pos=(%.3f, %.3f, %.3f) version=%llu dirty=%d",
                         owner_id.index,
                         static_cast<uint32_t>(storageHandle),
                         pos.x,
                         pos.y,
                         pos.z,
                         static_cast<unsigned long long>(GetVersion()),
                         IsDirty() ? 1 : 0);
            }
        #endif//HGL_TRANSFORM_DEBUG_LOGGING
        }

        glm::quat TransformComponent::GetLocalRotation() const
        {
            return GetAccessor().GetLocalRotation();
        }

        void TransformComponent::SetLocalRotation(const glm::quat& rot)
        {
            // D4：运行期写 Static 物体 → 一次性告警（构造期写入不计）
            WarnStaticRuntimeWrite("SetLocalRotation");
            GetAccessor().SetLocalRotation(rot);
            MarkDirty(ToChangeMask(TransformChange::Rotation));

        #if HGL_TRANSFORM_DEBUG_LOGGING
            static uint32_t s_rot_log_tick = 0;
            ++s_rot_log_tick;
            if ((s_rot_log_tick % 180u) == 1u)
            {
                GLogInfo("[TransformComponent] SetLocalRotation: owner=%u handle=%u rot=(%.3f, %.3f, %.3f, %.3f) version=%llu dirty=%d",
                         owner_id.index,
                         static_cast<uint32_t>(storageHandle),
                         rot.w,
                         rot.x,
                         rot.y,
                         rot.z,
                         static_cast<unsigned long long>(GetVersion()),
                         IsDirty() ? 1 : 0);
            }
        #endif//HGL_TRANSFORM_DEBUG_LOGGING
        }

        glm::vec3 TransformComponent::GetLocalScale() const
        {
            return GetAccessor().GetLocalScale();
        }

        void TransformComponent::SetLocalScale(const glm::vec3& scale)
        {
            // D4：运行期写 Static 物体 → 一次性告警（构造期写入不计）
            WarnStaticRuntimeWrite("SetLocalScale");
            GetAccessor().SetLocalScale(scale);
            MarkDirty(ToChangeMask(TransformChange::Scale));

        #if HGL_TRANSFORM_DEBUG_LOGGING
            static uint32_t s_scale_log_tick = 0;
            ++s_scale_log_tick;
            if ((s_scale_log_tick % 180u) == 1u)
            {
                GLogInfo("[TransformComponent] SetLocalScale: owner=%u handle=%u scale=(%.3f, %.3f, %.3f) version=%llu dirty=%d",
                         owner_id.index,
                         static_cast<uint32_t>(storageHandle),
                         scale.x,
                         scale.y,
                         scale.z,
                         static_cast<unsigned long long>(GetVersion()),
                         IsDirty() ? 1 : 0);
            }
        #endif//HGL_TRANSFORM_DEBUG_LOGGING
        }

        void TransformComponent::SetLocalTRS(const glm::vec3& pos, const glm::quat& rot, const glm::vec3& scale)
        {
            // D4：运行期写 Static 物体 → 一次性告警（构造期写入不计）
            WarnStaticRuntimeWrite("SetLocalTRS");
            GetAccessor().SetLocalTRS(pos, rot, scale);
            MarkDirty(ToChangeMask(TransformChange::LocalTRS));
        }

        glm::mat4 TransformComponent::GetWorldMatrix()
        {
            return GetAccessor().GetWorldMatrix();
        }

        bool TransformComponent::IsDirty() const
        {
            return GetAccessor().IsDirty();
        }

        TransformAccessor TransformComponent::GetAccessor() const
        {
            return TransformAccessor(GetStorage(), GetStorageHandle(), owner_context);
        }

        uint64_t TransformComponent::GetVersion() const
        {
            return GetAccessor().GetVersion();
        }

        uint32_t TransformComponent::GetChangeMask() const
        {
            return GetAccessor().GetChangeMask();
        }

        void TransformComponent::ArmStaticRuntimeWriteWarning()
        {
            GetAccessor().ArmWriteWarning();
        }

        bool TransformComponent::IsStaticRuntimeWriteArmed() const
        {
            return GetAccessor().IsWriteArmed();
        }

        bool TransformComponent::HasWarnedStaticRuntimeWrite() const
        {
            return GetAccessor().HasWarnedWrite();
        }

        glm::vec3 TransformComponent::GetWorldPosition()
        {
            return GetAccessor().GetWorldPosition();
        }

        void TransformComponent::SetWorldPosition(const glm::vec3& pos)
        {
            // D4：运行期写 Static 物体 → 一次性告警（构造期写入不计）
            WarnStaticRuntimeWrite("SetWorldPosition");
            GetAccessor().SetWorldPosition(pos);
            MarkDirty(ToChangeMask(TransformChange::Position));
        }

        glm::quat TransformComponent::GetWorldRotation()
        {
            return GetAccessor().GetWorldRotation();
        }

        void TransformComponent::SetWorldRotation(const glm::quat& rot)
        {
            // D4：运行期写 Static 物体 → 一次性告警（构造期写入不计）
            WarnStaticRuntimeWrite("SetWorldRotation");
            GetAccessor().SetWorldRotation(rot);
            MarkDirty(ToChangeMask(TransformChange::Rotation));
        }

        glm::vec3 TransformComponent::GetWorldScale()
        {
            return GetAccessor().GetWorldScale();
        }

        void TransformComponent::SetWorldScale(const glm::vec3& scale)
        {
            // D4：运行期写 Static 物体 → 一次性告警（构造期写入不计）
            WarnStaticRuntimeWrite("SetWorldScale");
            GetAccessor().SetWorldScale(scale);
            MarkDirty(ToChangeMask(TransformChange::Scale));
        }

        float TransformComponent::ComputeWorldUnitsPerPixel(const hgl::graph::CameraInfo* camera_info,
                                                            const hgl::graph::ViewportInfo* viewport_info)
        {
            if (!camera_info || !viewport_info)
                return 0.0f;

            const float viewport_height = float(viewport_info->GetViewportHeight() > 0 ? viewport_info->GetViewportHeight() : 1u);
            const float proj_11 = std::fabs(camera_info->projection[1][1]);
            if (proj_11 <= 1e-6f)
                return 0.0f;

            const bool is_ortho = std::fabs(camera_info->projection[3][3] - 1.0f) < 1e-6f;

            if (is_ortho)
            {
                const float world_height = 2.0f / proj_11;
                return world_height / viewport_height;
            }

            const float tan_half_fovy = 1.0f / proj_11;
            const glm::vec3 world_pos = GetWorldPosition();
            glm::vec3 to_object = world_pos - camera_info->pos;

            float depth = std::fabs(glm::dot(to_object, camera_info->view_line));
            if (depth <= 1e-4f)
                depth = glm::length(to_object);
            if (depth <= 1e-4f)
                depth = 1.0f;

            const float world_height = 2.0f * depth * tan_half_fovy;
            return world_height / viewport_height;
        }

        float TransformComponent::ComputeFixedPixelUniformScale(const hgl::graph::CameraInfo* camera_info,
                                                                const hgl::graph::ViewportInfo* viewport_info,
                                                                float pixel_diameter,
                                                                float reference_world_diameter)
        {
            if (pixel_diameter <= 0.0f || reference_world_diameter <= 1e-6f)
                return 0.0f;

            const float world_per_pixel = ComputeWorldUnitsPerPixel(camera_info, viewport_info);
            if (world_per_pixel <= 0.0f)
                return 0.0f;

            const float target_world_diameter = pixel_diameter * world_per_pixel;
            return target_world_diameter / reference_world_diameter;
        }

        bool TransformComponent::ApplyFixedPixelUniformScale(const hgl::graph::CameraInfo* camera_info,
                                                             const hgl::graph::ViewportInfo* viewport_info,
                                                             float pixel_diameter,
                                                             float reference_world_diameter,
                                                             float min_scale)
        {
            float scale = ComputeFixedPixelUniformScale(camera_info,
                                                        viewport_info,
                                                        pixel_diameter,
                                                        reference_world_diameter);
            if (scale <= 0.0f)
                return false;

            if (scale < min_scale)
                scale = min_scale;

            const glm::vec3 current_scale = GetLocalScale();
            if (std::fabs(current_scale.x - scale) > 1e-5f ||
                std::fabs(current_scale.y - scale) > 1e-5f ||
                std::fabs(current_scale.z - scale) > 1e-5f)
            {
                SetLocalScale(glm::vec3(scale));
            }

            return true;
        }

        void TransformComponent::SetFixedPixelSizingEnabled(bool enabled)
        {
            fixed_pixel_sizing_enabled = enabled;
        }

        void TransformComponent::SetFixedPixelSizingParameters(float pixel_diameter,
                                                               float reference_world_diameter,
                                                               float min_scale)
        {
            if (pixel_diameter > 0.0f)
                fixed_pixel_diameter = pixel_diameter;

            if (reference_world_diameter > 1e-6f)
                fixed_pixel_reference_world_diameter = reference_world_diameter;

            if (min_scale > 0.0f)
                fixed_pixel_min_scale = min_scale;
        }

        void TransformComponent::SetFixedPixelSizingContext(const hgl::graph::CameraInfo* camera_info,
                                                            const hgl::graph::ViewportInfo* viewport_info)
        {
            fixed_pixel_camera_info = camera_info;
            fixed_pixel_viewport_info = viewport_info;

            if (fixed_pixel_sizing_enabled && fixed_pixel_camera_info && fixed_pixel_viewport_info)
            {
                ApplyFixedPixelUniformScale(fixed_pixel_camera_info,
                                            fixed_pixel_viewport_info,
                                            fixed_pixel_diameter,
                                            fixed_pixel_reference_world_diameter,
                                            fixed_pixel_min_scale);
            }
        }

        void TransformComponent::SetParent(EntityID parent)
        {
            WarnStaticRuntimeWrite("SetParent");

            auto *storage = GetStorage();
            const auto self = GetStorageHandle();

            // 从旧父的子表里摘掉自己（父链与子表真源都在存储）
            if (parent_id.IsValid())
            {
                const TransformID old_parent = storage->FindByOwner(parent_id);

                if (IsValidTransformID(old_parent))
                    storage->RemoveChild(old_parent, self);
            }

            parent_id = parent;

            // 接上新父
            TransformID parent_tid = INVALID_TRANSFORM_ID;

            if (parent.IsValid())
            {
                parent_tid = storage->FindByOwner(parent);

                if (IsValidTransformID(parent_tid))
                    storage->AddChild(parent_tid, self);
            }

            storage->SetParent(self, parent_tid);

            MarkDirty(ToChangeMask(TransformChange::Parent) | ToChangeMask(TransformChange::WorldMatrix));
        }

        Entity* TransformComponent::GetParent() const
        {
            if (!owner_context || !parent_id.IsValid())
                return nullptr;
            return owner_context->GetEntity(parent_id);
        }

        void TransformComponent::AddChild(EntityID child)
        {
            if (!child.IsValid())
                return;

            auto *storage = GetStorage();
            const TransformID child_tid = storage->FindByOwner(child);

            if (IsValidTransformID(child_tid))
                storage->AddChild(GetStorageHandle(), child_tid);
        }

        void TransformComponent::RemoveChild(EntityID child)
        {
            if (!child.IsValid())
                return;

            auto *storage = GetStorage();
            const TransformID child_tid = storage->FindByOwner(child);

            if (IsValidTransformID(child_tid))
                storage->RemoveChild(GetStorageHandle(), child_tid);
        }

        void TransformComponent::GetChildEntities(std::vector<Entity*>& out) const
        {
            out.clear();

            if (!owner_context)
                return;

            auto *storage = GetStorage();

            for (const TransformID child : storage->GetChildren(GetStorageHandle()))
            {
                const EntityID child_owner = storage->GetOwner(child);

                if (!child_owner.IsValid())
                    continue;

                Entity* entity = owner_context->GetEntity(child_owner);
                if (entity)
                    out.push_back(entity);
            }
        }

        void TransformComponent::SetMobility(Mobility new_mobility)
        {
            MigrateStorage(static_cast<Mobility>(new_mobility));
        }

        void TransformComponent::SetMovable(bool isMovable)
        {
            SetMobility(isMovable ? Mobility::Movable : Mobility::Static);
        }

        // 原 OnUpdate 每帧重算 fixed_pixel 的通道已删除：gizmo 的
        // TransformGizmoSystem 在 TickPostCamera 相位每帧调
        // SetFixedPixelSizingContext（设置时即时 Apply），是主通道。

        void TransformComponent::OnAttach()
        {
            if (owner_context)
            {
                auto* target_storage = owner_context->GetTransformStorage();
                if (target_storage)
                {
                    if (bound_storage != target_storage)
                    {
                        // 换存储：旧行里的 TRS 是真源，必须搬到新行（组件侧没有副本可抄）
                        glm::vec3 pos(0.0f);
                        glm::quat rot(1.0f, 0.0f, 0.0f, 0.0f);
                        glm::vec3 scale(1.0f);

                        if (bound_storage && storageHandle != TransformDataStorage::INVALID_HANDLE)
                        {
                            pos   = bound_storage->GetPosition(storageHandle);
                            rot   = bound_storage->GetRotation(storageHandle);
                            scale = bound_storage->GetScale(storageHandle);
                            bound_storage->Deallocate(storageHandle);
                        }

                        bound_storage = target_storage;
                        storageHandle = target_storage->Allocate();
                        target_storage->SetLocalTRS(storageHandle, pos, rot, scale);
                    }
                    target_storage->SetOwner(storageHandle, owner_id);
                    target_storage->SetMobility(storageHandle, IsMovable() ? 1 : 0);

                    if (parent_id.IsValid())
                    {
                        const TransformID parent_tid = target_storage->FindByOwner(parent_id);

                        if (IsValidTransformID(parent_tid))
                        {
                            target_storage->SetParent(storageHandle, parent_tid);
                            target_storage->AddChild(parent_tid, storageHandle);
                        }
                    }
                }
            }

            MarkDirty(ToChangeMask(TransformChange::LocalTRS));

            // Register with context
            if (auto owner = GetOwner())
            {
                if (auto ctx = owner->GetContext())
                {
                    ctx->RegisterTransform(storageHandle, IsMovable());
                }
            }
        }

        void TransformComponent::OnDetach()
        {
            // Unregister from context
            if (auto owner = GetOwner())
            {
                if (auto ctx = owner->GetContext())
                {
                    ctx->UnregisterTransform(storageHandle);
                }
            }

            // 先从父的子表里摘掉自己（存储是真源），再释放本行
            if (bound_storage && storageHandle != TransformDataStorage::INVALID_HANDLE && parent_id.IsValid())
            {
                const TransformID parent_tid = bound_storage->FindByOwner(parent_id);

                if (IsValidTransformID(parent_tid))
                    bound_storage->RemoveChild(parent_tid, storageHandle);
            }

            if (bound_storage && storageHandle != TransformDataStorage::INVALID_HANDLE)
            {
                bound_storage->Deallocate(storageHandle);      // 行内的子表/owner 反向索引一并清掉
                storageHandle = TransformDataStorage::INVALID_HANDLE;
                bound_storage = nullptr;
            }
        }

        void TransformComponent::UpdateWorldMatrix()
        {
            GetAccessor().UpdateIfDirty();
            GetAccessor().AddChangeMask(ToChangeMask(TransformChange::WorldMatrix));
        }

        void TransformComponent::UpdateIfDirty()
        {
            if (!IsDirty())
                return;

            UpdateWorldMatrix();
        }

        // D4：运行期写 Static 物体的一次性告警。
        // 触发条件 = 当前是 Static 且组件已被渲染侧消费过（armed）；场景搭建期
        // （首次 SubmitTransformUpdates 之前）的写入不算"运行期"，故不告警。
        // 每组件只报一次：真正的误用是"每帧写"，一次性告警足以暴露，不该刷屏。
        void TransformComponent::WarnStaticRuntimeWrite(const char *what)
        {
            auto accessor = GetAccessor();

            if (!IsStatic() || !accessor.IsWriteArmed() || accessor.HasWarnedWrite())
                return;

            accessor.SetWriteWarned();

            const char *entity_name = "<no-owner>";
            if (Entity *owner = GetOwner())
                entity_name = owner->GetName().c_str();

            GLogWarning(u8"[TransformComponent] 运行期写入 Static transform"
                        u8"（%s｜实体 '%s' id=%u）：静态物体写入会整段重写静态矩阵并让"
                        u8"**全部**静态级联缓存失效（当帧 4 级全量重绘），且连带标脏整棵子树。"
                        u8"该对象若会动 ⇒ 创建期就用 SetMobility(Mobility::Movable)"
                        u8"（迁到 movable 通道：每帧 ring 写 + 动态级联）；"
                        u8"编辑期一次性调整可忽略——本告警每个组件只报一次。",
                        what, entity_name,
                        static_cast<uint32_t>(owner_id.index));
        }

        void TransformComponent::MarkDirty()
        {
            MarkDirty(ToChangeMask(TransformChange::WorldMatrix));
        }

        void TransformComponent::MarkDirty(uint32_t change_mask)
        {
            GetAccessor().TouchChange(change_mask);
            GetAccessor().MarkDirty();

            MarkDescendantsDirty();
        }

        // 孩子/孙子的世界矩阵都变了 ⇒ 逐行记一次"WorldMatrix 变更"。
        // 存储的平铺求值只负责算出新矩阵，"哪些行要重传上 GPU"是由**版本号比对**决定的，
        // 所以这里必须逐行 bump（只标脏不 bump 版本 ⇒ 子节点的 L2W 行不会重传）。
        void TransformComponent::MarkDescendantsDirty()
        {
            auto *storage = GetStorage();

            std::vector<TransformID> stack(GetAccessor().GetChildren());

            while (!stack.empty())
            {
                const TransformID id = stack.back();
                stack.pop_back();

                if (!IsValidTransformID(id) || id >= static_cast<TransformID>(storage->GetCount()))
                    continue;

                storage->SetDirty(id, true);
                storage->TouchChange(id, ToChangeMask(TransformChange::WorldMatrix));

                for (const TransformID child : storage->GetChildren(id))
                    stack.push_back(child);
            }
        }

        TransformDataStorage* TransformComponent::GetStorage() const
        {
            if (bound_storage)
                return bound_storage;
            if (owner_context)
            {
                if (auto* s = owner_context->GetTransformStorage())
                    return s;
            }
            return GetSharedStorage().get();
        }

        void TransformComponent::MigrateStorage(Mobility target_mobility)
        {
            if (mobility == target_mobility)
                return;

            const bool to_movable = (target_mobility == Mobility::Movable);

            TouchChange(ToChangeMask(TransformChange::Mobility));

            if (storageHandle == TransformDataStorage::INVALID_HANDLE)
            {
                mobility = target_mobility;
                return;
            }

            GetAccessor().SetMobility(target_mobility);
            mobility = target_mobility;

            // If transitioning to static and dirty, compute world matrix once
            if (!to_movable && IsDirty())
            {
                UpdateWorldMatrix();
            }

            // Notify context of migration
            if (auto owner = GetOwner())
            {
                if (auto ctx = owner->GetContext())
                {
                    ctx->MigrateTransform(storageHandle, to_movable);
                }
            }
        }
    }//namespace ecs
}//namespace hgl

