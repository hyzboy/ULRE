#include<hgl/ecs/support/TransformAccessor.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/graph/CameraInfo.h>
#include<hgl/graph/ubo/ViewportInfo.h>

#include<glm/gtc/matrix_transform.hpp>
#include<cmath>

namespace hgl
{
    namespace ecs
    {
        namespace
        {
            /// 父链遍历的安全上限（正常层级远小于此；只为防脏数据成环）
            constexpr int kMaxHierarchyWalk = 256;

            inline bool InRange(const TransformDataStorage *storage,TransformID id)
            {
                return storage && id != INVALID_TRANSFORM_ID && id < static_cast<TransformID>(storage->GetCount());
            }
        }//namespace

        Entity *TransformAccessor::GetOwner() const
        {
            ECSContext *ctx = GetContext();          // 由 storage 反查，句柄不再自带

            if (!ctx)
                return nullptr;

            const EntityID owner = GetOwnerID();

            if (!owner.IsValid())
                return nullptr;

            return ctx->GetEntity(owner);
        }

        EntityID TransformAccessor::GetOwnerID() const
        {
            return IsValid() ? storage->GetOwner(id) : EntityID();
        }

        // ── 元数据（变更版本 / D4 标记）──────────────────────────────────────

        uint32_t TransformAccessor::GetChangeMask() const
        {
            return IsValid() ? storage->GetChangeMask(id) : 0;
        }

        void TransformAccessor::ClearChangeMask() const
        {
            if (IsValid())
                storage->ClearChangeMask(id);
        }

        void TransformAccessor::TouchChange(uint32_t mask) const
        {
            if (IsValid())
                storage->TouchChange(id,mask);
        }

        void TransformAccessor::AddChangeMask(uint32_t mask) const
        {
            if (IsValid())
                storage->AddChangeMask(id,mask);
        }

        // 写入本行后的记账。T8 起调用面只剩访问器 ⇒ 必须在这里做（原先由组件侧的
        // 标脏 + 子孙标脏承担；那套组件侧记账已删），否则"只经访问器写入"的行 change_mask 恒为 0，
        // `TransformSystem::ShouldUpdateTransform` 的掩码判据会让它**永不上传**。
        void TransformAccessor::MarkLocalChanged(uint32_t change_mask) const
        {
            if (!IsValid())
                return;

            storage->TouchChange(id,change_mask);
            storage->SetDirty(id,true);

            // 子/孙的世界矩阵都变了 ⇒ 逐行 bump 版本 + 记 WorldMatrix 变更
            std::vector<TransformID> stack;
            storage->CopyChildren(id,stack);

            // 2026-10-11（审计 · 批 1.1 B2）防御性终止闸：`SetParent` 已在入口拒环，但若有人
            // 绕过访问器、直接用原始存储 API 造出环（`Test 6` 那条容忍契约），这里必须能停下来。
            // 正常树/菱形传播的步数远小于「行数 × 4 + 64」；超出即视为环，报一次错后停止传播。
            const uint64_t max_steps = static_cast<uint64_t>(storage->GetCount()) * 4u + 64u;
            uint64_t steps = 0;

            while (!stack.empty())
            {
                if (++steps > max_steps)
                {
                    GLogError(u8"[TransformAccessor] 后代标脏遍历步数异常（>%llu）—— 疑似父子成环，已停止传播"
                              u8"（环数据应由 SetParent 拒绝；绕过 API 写入属越界使用）",
                              static_cast<unsigned long long>(max_steps));
                    break;
                }

                const TransformID child = stack.back();
                stack.pop_back();

                if (!IsValidTransformID(child) || child >= static_cast<TransformID>(storage->GetCount()))
                    continue;

                storage->SetDirty(child,true);
                storage->TouchChange(child,ToChangeMask(TransformChange::WorldMatrix));

                std::vector<TransformID> grand;
                storage->CopyChildren(child,grand);
                stack.insert(stack.end(),grand.begin(),grand.end());
            }
        }

        // D4：运行期写 Static 物体的一次性告警。
        // 触发条件 = 当前是 Static 且该行已被渲染侧消费过（armed）；场景搭建期
        // （首次 SubmitTransformUpdates 之前）的写入不算"运行期"，故不告警。
        // 每行只报一次：真正的误用是"每帧写"，一次性告警足以暴露，不该刷屏。
        void TransformAccessor::WarnStaticRuntimeWrite(const char *what) const
        {
            if (!IsValid() || !IsStatic() || !IsWriteArmed() || HasWarnedWrite())
                return;

            SetWriteWarned();

            const char *entity_name = "<no-owner>";
            if (Entity *owner = GetOwner())
                entity_name = owner->GetName().c_str();

            GLogWarning(u8"[Transform] 运行期写入 Static transform"
                        u8"（%s｜实体 '%s' id=%u）：静态物体写入会整段重写静态矩阵并让"
                        u8"**全部**静态级联缓存失效（当帧 4 级全量重绘），且连带标脏整棵子树。"
                        u8"该对象若会动 ⇒ 创建期就用 Mobility::Movable"
                        u8"（迁到 movable 通道：每帧 ring 写 + 动态级联）；"
                        u8"编辑期一次性调整可忽略——本告警每个变换行只报一次。",
                        what, entity_name,
                        static_cast<uint32_t>(GetOwnerID().index));
        }

        uint64_t TransformAccessor::GetVersion() const
        {
            return IsValid() ? storage->GetVersion(id) : 0;
        }

        bool TransformAccessor::IsWriteArmed() const
        {
            return IsValid() && storage->IsWriteArmed(id);
        }

        void TransformAccessor::ArmWriteWarning() const
        {
            if (IsValid())
                storage->ArmWrite(id);
        }

        bool TransformAccessor::HasWarnedWrite() const
        {
            return IsValid() && storage->HasWarnedWrite(id);
        }

        void TransformAccessor::SetWriteWarned() const
        {
            if (IsValid())
                storage->SetWriteWarned(id);
        }

        // ── 局部 TRS：读写直落存储（唯一真源）────────────────────────────────

        glm::vec3 TransformAccessor::GetLocalPosition() const
        {
            return IsValid() ? storage->GetPosition(id) : glm::vec3(0.0f);
        }

        void TransformAccessor::SetLocalPosition(const glm::vec3 &pos) const
        {
            if (!IsValid())
                return;

            WarnStaticRuntimeWrite("SetLocalPosition");
            storage->SetPosition(id,pos);
            MarkLocalChanged(ToChangeMask(TransformChange::Position));
        }

        glm::quat TransformAccessor::GetLocalRotation() const
        {
            return IsValid() ? storage->GetRotation(id) : glm::quat(1.0f,0.0f,0.0f,0.0f);
        }

        void TransformAccessor::SetLocalRotation(const glm::quat &rot) const
        {
            if (!IsValid())
                return;

            WarnStaticRuntimeWrite("SetLocalRotation");
            storage->SetRotation(id,rot);
            MarkLocalChanged(ToChangeMask(TransformChange::Rotation));
        }

        glm::vec3 TransformAccessor::GetLocalScale() const
        {
            return IsValid() ? storage->GetScale(id) : glm::vec3(1.0f);
        }

        void TransformAccessor::SetLocalScale(const glm::vec3 &scale) const
        {
            if (!IsValid())
                return;

            WarnStaticRuntimeWrite("SetLocalScale");
            storage->SetScale(id,scale);
            MarkLocalChanged(ToChangeMask(TransformChange::Scale));
        }

        void TransformAccessor::SetLocalTRS(const glm::vec3 &pos,const glm::quat &rot,const glm::vec3 &scale) const
        {
            if (!IsValid())
                return;

            WarnStaticRuntimeWrite("SetLocalTRS");
            storage->SetLocalTRS(id,pos,rot,scale);
            MarkLocalChanged(ToChangeMask(TransformChange::LocalTRS));
        }

        // ── 层级：父链真源在存储 ──────────────────────────────────────────────

        TransformID TransformAccessor::GetParent() const
        {
            return IsValid() ? storage->GetParent(id) : INVALID_TRANSFORM_ID;
        }

        void TransformAccessor::SetParent(TransformID parent) const
        {
            if (!IsValid())
                return;

            WarnStaticRuntimeWrite("SetParent");

            const TransformID new_parent = (parent == id) ? INVALID_TRANSFORM_ID : parent;   // 自我成环直接忽略

            // 2026-10-11（审计 · 批 1.1 B2）：**祖先环检测**。原实现只拒自环，
            // `A.SetParent(B)` 后再 `B.SetParent(A)` 就造出环，而下面的 `MarkLocalChanged`
            // 后代遍历既无 visited 也无深度上限 ⇒ 探针实测第二次调用 >3s 不返回（真死循环）。
            // 判决点选在**访问器**（公共 API）而不是存储层：`TransformDataStorage::SetParent`
            // 保持容忍环，因为 `Test 6（Cycle protection）` 钉的正是「原始数据层能容忍环、
            // 拓扑重建不崩」这条契约 —— 数据层不该替调用方做策略判断。
            // 代价 O(depth)，且只在真正改父时付；必须在动父链/子表**之前**判，否则会留下半改状态。
            if (IsValidTransformID(new_parent))
            {
                const uint32_t max_steps = static_cast<uint32_t>(storage->GetCount()) + 1u;
                TransformID walker = new_parent;

                for (uint32_t steps = 0; walker != INVALID_TRANSFORM_ID; ++steps)
                {
                    if (walker == id || steps > max_steps)
                    {
                        GLogError(u8"[TransformAccessor] SetParent 拒绝成环：%u 认 %u 为父将与祖先链相撞"
                                  u8"（环上没有终止条件 ⇒ 后代标脏会死循环）—— 父链与子表均保持不变",
                                  static_cast<uint32_t>(id), static_cast<uint32_t>(new_parent));
                        return;
                    }

                    walker = storage->GetParent(walker);
                }
            }

            // 父链与**子表**都要维护：子表是真源的一部分，子孙标脏/版本 bump 靠它遍历
            const TransformID old_parent = storage->GetParent(id);

            if (IsValidTransformID(old_parent))
                storage->RemoveChild(old_parent,id);

            if (IsValidTransformID(new_parent))
                storage->AddChild(new_parent,id);

            storage->SetParent(id,new_parent);

            MarkLocalChanged(ToChangeMask(TransformChange::Parent) | ToChangeMask(TransformChange::WorldMatrix));
        }

        void TransformAccessor::AddChild(TransformID child) const
        {
            if (IsValid())
                storage->AddChild(id,child);
        }

        void TransformAccessor::RemoveChild(TransformID child) const
        {
            if (IsValid())
                storage->RemoveChild(id,child);
        }

        // ── 世界变换：派生量（局部 TRS + 父链组合）────────────────────────────

        glm::mat4 TransformAccessor::GetWorldMatrix() const
        {
            if (!IsValid())
                return glm::mat4(1.0f);

            if (storage->IsDirty(id) || storage->IsTopologyDirty())
                storage->UpdateDirtyWorldMatricesFlat();

            return storage->GetWorldMatrix(id);
        }

        glm::vec3 TransformAccessor::GetWorldPosition() const
        {
            return glm::vec3(GetWorldMatrix()[3]);
        }

        void TransformAccessor::SetWorldPosition(const glm::vec3 &pos) const
        {
            if (!IsValid())
                return;

            const TransformID parent = storage->GetParent(id);

            if (InRange(storage,parent))
            {
                const glm::mat4 parent_world = TransformAccessor(storage,parent).GetWorldMatrix();
                storage->SetPosition(id,glm::vec3(glm::inverse(parent_world) * glm::vec4(pos,1.0f)));
            }
            else
                storage->SetPosition(id,pos);

            WarnStaticRuntimeWrite("SetWorldPosition");
            MarkLocalChanged(ToChangeMask(TransformChange::Position));
        }

        glm::quat TransformAccessor::GetWorldRotation() const
        {
            if (!IsValid())
                return glm::quat(1.0f,0.0f,0.0f,0.0f);

            glm::quat world = storage->GetRotation(id);
            TransformID parent = storage->GetParent(id);

            for (int guard = 0; InRange(storage,parent) && guard < kMaxHierarchyWalk; ++guard)
            {
                world  = storage->GetRotation(parent) * world;
                parent = storage->GetParent(parent);
            }

            return world;
        }

        void TransformAccessor::SetWorldRotation(const glm::quat &rot) const
        {
            if (!IsValid())
                return;

            const TransformID parent = storage->GetParent(id);

            if (InRange(storage,parent))
            {
                const glm::quat parent_world = TransformAccessor(storage,parent).GetWorldRotation();
                storage->SetRotation(id,glm::inverse(parent_world) * rot);
            }
            else
                storage->SetRotation(id,rot);

            WarnStaticRuntimeWrite("SetWorldRotation");
            MarkLocalChanged(ToChangeMask(TransformChange::Rotation));
        }

        glm::vec3 TransformAccessor::GetWorldScale() const
        {
            if (!IsValid())
                return glm::vec3(1.0f);

            glm::vec3 world = storage->GetScale(id);
            TransformID parent = storage->GetParent(id);

            for (int guard = 0; InRange(storage,parent) && guard < kMaxHierarchyWalk; ++guard)
            {
                world  = storage->GetScale(parent) * world;
                parent = storage->GetParent(parent);
            }

            return world;
        }

        void TransformAccessor::SetWorldScale(const glm::vec3 &scale) const
        {
            if (!IsValid())
                return;

            const TransformID parent = storage->GetParent(id);

            if (InRange(storage,parent))
            {
                const glm::vec3 parent_world = TransformAccessor(storage,parent).GetWorldScale();
                storage->SetScale(id,scale / parent_world);
            }
            else
                storage->SetScale(id,scale);

            WarnStaticRuntimeWrite("SetWorldScale");
            MarkLocalChanged(ToChangeMask(TransformChange::Scale));
        }

        // ── 表现层：fixed-pixel 尺寸控制 ──────────────────────────────────────

        void TransformAccessor::SetFixedPixelSizingEnabled(bool enabled) const
        {
            if (!IsValid())
                return;

            auto state = storage->GetFixedPixel(id);

            state.enabled = enabled;
            storage->SetFixedPixel(id,state);
        }

        bool TransformAccessor::IsFixedPixelSizingEnabled() const
        {
            return IsValid() ? storage->GetFixedPixel(id).enabled : false;
        }

        void TransformAccessor::SetFixedPixelSizingParameters(float pixel_diameter,float reference_world_diameter,float min_scale) const
        {
            if (!IsValid())
                return;

            auto state = storage->GetFixedPixel(id);

            if (pixel_diameter > 0.0f)
                state.diameter = pixel_diameter;

            if (reference_world_diameter > 1e-6f)
                state.reference_world_diameter = reference_world_diameter;

            if (min_scale > 0.0f)
                state.min_scale = min_scale;

            storage->SetFixedPixel(id,state);
        }

        void TransformAccessor::SetFixedPixelSizingContextAndApply(const hgl::graph::CameraInfo *camera_info,const hgl::graph::ViewportInfo *viewport_info) const
        {
            if (!IsValid())
                return;

            auto state = storage->GetFixedPixel(id);

            state.camera_info   = camera_info;
            state.viewport_info = viewport_info;

            storage->SetFixedPixel(id,state);

            // 设置时即时 Apply（gizmo 依赖"每帧给上下文并立刻生效"这条路径）
            if (state.enabled && state.camera_info && state.viewport_info)
            {
                ApplyFixedPixelUniformScale(state.camera_info,
                                            state.viewport_info,
                                            state.diameter,
                                            state.reference_world_diameter,
                                            state.min_scale);
            }
        }

        float TransformAccessor::ComputeWorldUnitsPerPixel(const hgl::graph::CameraInfo *camera_info,const hgl::graph::ViewportInfo *viewport_info) const
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

        float TransformAccessor::ComputeFixedPixelUniformScale(const hgl::graph::CameraInfo *camera_info,const hgl::graph::ViewportInfo *viewport_info,float pixel_diameter,float reference_world_diameter) const
        {
            if (pixel_diameter <= 0.0f || reference_world_diameter <= 1e-6f)
                return 0.0f;

            const float world_per_pixel = ComputeWorldUnitsPerPixel(camera_info,viewport_info);
            if (world_per_pixel <= 0.0f)
                return 0.0f;

            const float target_world_diameter = pixel_diameter * world_per_pixel;
            return target_world_diameter / reference_world_diameter;
        }

        bool TransformAccessor::ApplyFixedPixelUniformScale(const hgl::graph::CameraInfo *camera_info,const hgl::graph::ViewportInfo *viewport_info,float pixel_diameter,float reference_world_diameter,float min_scale) const
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

        // ── 移动性 / 脏标记 ──────────────────────────────────────────────────

        Mobility TransformAccessor::GetMobility() const
        {
            if (!IsValid())
                return Mobility::Static;

            return storage->GetMobility(id) ? Mobility::Movable : Mobility::Static;
        }

        void TransformAccessor::SetMobility(Mobility m) const
        {
            if (!IsValid())
                return;

            const bool to_movable = (m == Mobility::Movable);

            if (IsMovable() == to_movable)
                return;

            // 2026-10-11（审计 · 批 1.1 B3）：**先换段、成功后才改真源**。
            // 原顺序是先 `storage->SetMobility(...)` 再通知世界换段，而换段可能被拒绝（目标段满）
            // ⇒ storage 说「可动」、段名单还在静态段。现在换段成功时才写 mobility（世界侧唯一写入点），
            // 失败即原状返回。
            //
            // 渲染侧的索引映射与上传通道都按段名单分流——只改存储里的 mobility 字节、不换段的话，
            // 该行会停在旧通道：索引取不到 ⇒ 实例拿到默认行（单位矩阵）⇒ 全画在原点。
            ECSContext *migrate_world = GetContext();

            if (migrate_world)
            {
                if (!migrate_world->MigrateTransform(id,to_movable))
                {
                    GLogWarning("[TransformAccessor] SetMobility 被拒绝（目标段预算用尽）—— mobility 与段名单均保持原状：%u",
                                static_cast<uint32_t>(id));
                    return;
                }
            }
            else
            {
                storage->SetMobility(id,to_movable ? 1 : 0);   // 无世界（纯存储用法）：没有段可换，只改真源
            }

            MarkLocalChanged(ToChangeMask(TransformChange::Mobility));

            // 迁到静态通道前先把世界矩阵算一次（冻结前的最后状态必须正确）
            if (!to_movable)
                UpdateIfDirty();
        }

        bool TransformAccessor::IsDirty() const
        {
            return IsValid() ? storage->IsDirty(id) : false;
        }

        // 2026-10-11（§3-1 选项①）：`TransformAccessor::MarkDirty()` 已删除 —— 只写 `matrixDirty`、
        // 不 bump `versions` ⇒ 静态行按版本比对的上传路径永不重传（而批 1.1 B4 之后它对静态行还会
        // `++static_change_seq`，于是"阴影缓存失效、矩阵仍旧"的半效状态更易误导）；动态行每帧全量写，
        // 该调用本就冗余。唯一调用点（`example/Transform/ClockUse.cpp`）上一行已 `SetLocalRotation`
        // （内部 bump 版本）⇒ 删除零行为变化。

        void TransformAccessor::UpdateIfDirty() const
        {
            if (IsValid() && (storage->IsDirty(id) || storage->IsTopologyDirty()))
                storage->UpdateDirtyWorldMatricesFlat();
        }
    }//namespace ecs
}//namespace hgl
