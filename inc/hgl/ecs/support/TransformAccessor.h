#pragma once

#include<hgl/ecs/support/TransformID.h>
#include<hgl/ecs/core/EntityHandle.h>
#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>

namespace hgl
{
    namespace graph
    {
        // 必须是 struct：与 CameraInfo.h / ubo/ViewportInfo.h 的定义一致。
        // 写成 class 会让本头文件里声明的成员函数被 mangle 成 PEV，
        // 与定义处的 PEU 不符 ⇒ LNK2019（C4099）。
        struct CameraInfo;
        struct ViewportInfo;
    }
}

namespace hgl
{
    namespace ecs
    {
        class ECSContext;
        class Entity;

        /**
         * 变换的**薄句柄**（值类型，**严格 16 字节** = storage* + id + generation，可随意拷贝）。
         *
         * 它只记「哪一行 + 哪个存储」，自身**不持有任何变换副本**：
         * 所有读写直落 `TransformDataStorage`（局部 TRS 唯一的真源），
         * 世界矩阵是派生量、由平铺求值算出（见 `UpdateDirtyWorldMatricesFlat`）。
         *
         * 所属世界**不落在句柄里**：`storage` 被其 `ECSContext` 独占拥有（`Context.h` 的
         * `unique_ptr`），句柄经 `storage->GetContext()` 反查即可 —— 刻意不留第二份 world 指针，
         * 因为"两个字段必须永远一致"本身就是一个静默失败源（不一致时 `SetMobility` 不换边）。
         *
         * 这是 T8（变换 `TransformID` 化）的调用面：
         * 调用点从「拿组件 shared_ptr」改成「拿 accessor 值」，语义不变。
         */
        class TransformAccessor
        {
            TransformDataStorage *storage = nullptr;
            TransformID           id      = INVALID_TRANSFORM_ID;
            uint32_t              generation = 0;       ///< 建柄时捕获的行世代（revision）：释放/重分配后旧柄自动失效

        public:

            TransformAccessor() = default;

            /// 建柄只收 `storage + id`（所属世界由存储反查）。**不提供**收 `ECSContext*` 的重载：
            /// 兼容层会把"忘了传世界"变成静默失败（`SetMobility` 不迁段 ⇒ 实例画在原点）。
            ///
            /// 建柄时**捕获行世代**：这是"句柄失效"的全部机制（v2 §2）。
            /// 世代编码：**0 = 死/未分配；正奇数 = 活**。释放（Deallocate）会把它归 0 ⇒ 旧柄与"死行上的新柄"都无效；
            /// 将来行复用时 +2 保持奇数 ⇒ ABA 免疫。
            TransformAccessor(TransformDataStorage *s,TransformID i)
                : storage(s),id(i),generation(s ? s->GetGeneration(i) : 0) {}

            bool IsValid() const
            {
                return storage
                    && id != INVALID_TRANSFORM_ID
                    && generation != 0                                  // 0 = 死/未分配：死行上的新柄也无效
                    && generation == storage->GetGeneration(id);        // 世代不符 = 该行已换过主人
            }

            TransformID           GetID() const         { return id; }
            TransformDataStorage *GetStorage() const    { return storage; }
            /// 所属世界（由存储反查；存储独立存在 / 未回填时为 nullptr）
            ECSContext           *GetContext() const    { return storage ? storage->GetContext() : nullptr; }

            /// 所属实体（真源在存储的 owners 行；不再由组件持有）
            EntityID GetOwnerID() const;

            /// 解析 owner 实体（所属世界为空、无 owner 或实体已销毁时返回 nullptr）
            Entity *GetOwner() const;

        public: // 局部 TRS（读写直落存储）

            glm::vec3 GetLocalPosition() const;
            void      SetLocalPosition(const glm::vec3 &pos) const;

            glm::quat GetLocalRotation() const;
            void      SetLocalRotation(const glm::quat &rot) const;

            glm::vec3 GetLocalScale() const;
            void      SetLocalScale(const glm::vec3 &scale) const;

            void      SetLocalTRS(const glm::vec3 &pos,const glm::quat &rot,const glm::vec3 &scale) const;

        public: // 世界变换（派生：局部 TRS + 父链组合）

            /// 需要时按脏标记做一次平铺求值（不逐帧全量重算）
            glm::mat4 GetWorldMatrix() const;

            glm::vec3 GetWorldPosition() const;
            void      SetWorldPosition(const glm::vec3 &pos) const;

            glm::quat GetWorldRotation() const;
            void      SetWorldRotation(const glm::quat &rot) const;

            glm::vec3 GetWorldScale() const;
            void      SetWorldScale(const glm::vec3 &scale) const;

        public: // 层级（父链真源在存储的 parent_indices / children）

            TransformID GetParent() const;
            void        SetParent(TransformID parent) const;

            void AddChild(TransformID child) const;
            void RemoveChild(TransformID child) const;

        public: // 元数据（owner / 变更版本 / D4 标记 —— 真源在存储）

            uint32_t GetChangeMask() const;
            void     ClearChangeMask() const;
            void     TouchChange(uint32_t mask) const;      ///< 版本 +1 且累积位掩码
            void     AddChangeMask(uint32_t mask) const;    ///< 仅累积位掩码（不改版本）
            uint64_t GetVersion() const;

            bool IsWriteArmed() const;                ///< D4：本行已被渲染侧消费过
            void ArmWriteWarning() const;
            bool HasWarnedWrite() const;
            void SetWriteWarned() const;

        public: // 表现层：fixed-pixel 尺寸控制（状态真源在存储，算法在这里）

            void SetFixedPixelSizingEnabled(bool enabled) const;
            bool IsFixedPixelSizingEnabled() const;
            void SetFixedPixelSizingParameters(float pixel_diameter,float reference_world_diameter,float min_scale=0.01f) const;
            void SetFixedPixelSizingContextAndApply(const hgl::graph::CameraInfo *camera_info,const hgl::graph::ViewportInfo *viewport_info) const;

            float ComputeWorldUnitsPerPixel(const hgl::graph::CameraInfo *camera_info,const hgl::graph::ViewportInfo *viewport_info) const;
            float ComputeFixedPixelUniformScale(const hgl::graph::CameraInfo *camera_info,const hgl::graph::ViewportInfo *viewport_info,float pixel_diameter,float reference_world_diameter) const;
            bool  ApplyFixedPixelUniformScale(const hgl::graph::CameraInfo *camera_info,const hgl::graph::ViewportInfo *viewport_info,float pixel_diameter,float reference_world_diameter,float min_scale=0.01f) const;

        public: // 移动性 / 脏标记

            Mobility GetMobility() const;
            void     SetMobility(Mobility m) const;
            bool     IsMovable() const { return GetMobility() == Mobility::Movable; }
            bool     IsStatic() const  { return GetMobility() == Mobility::Static; }

            bool IsDirty() const;
            void UpdateIfDirty() const;
            // 2026-10-11（§3-1 选项①）：`MarkDirty()` 已**删除**。它只写 `matrixDirty`、
            // 不 bump 版本 ⇒ 对静态行**永不上传**（上传判据是 `last_uploaded_version` 比对），
            // 对动态行则因每帧全量写而可有可无 ⇒ 名字承诺「刷新」、实际不保证，属语义陷阱。
            // 需要"变更记账 + 上传"请用 `SetLocal*/SetLocalScale` 一族（内部走 `MarkLocalChanged`）。

        private: // 内部：变更记账（掩码累积 + 子级版本 bump）与 D4「运行期写静态」告警

            /// 写入本行后记账：版本 +1、累积位掩码、标脏，并逐行 bump 子/孙的 WorldMatrix。
            /// （平铺求值只算矩阵；"哪些行重传 GPU"由版本号比对决定 ⇒ 只标脏不 bump，
            ///   子节点的 L2W 行不会重传。）
            void MarkLocalChanged(uint32_t change_mask) const;

            /// D4：运行期写 Static 物体的一次性告警（每行只报一次）。
            void WarnStaticRuntimeWrite(const char *what) const;
        };

        /// 句柄尺寸契约：**严格 16 字节**（storage* + id + generation）。
        /// 注意理由**不是**"两寄存器传参" —— MSVC x64 对 ≥12 B 的聚合体一律按引用传递
        /// （实测 `cl /O2 /FA`：12/16/24 B 结构体全部走隐藏指针；只有 ≤8 B 才落在 rcx）。
        /// 这条断言的意义是钉死"句柄里没有第二个冗余指针"（旧的 context* 已由 storage 反查替代）。
        static_assert(sizeof(TransformAccessor) == 16,
                      "TransformAccessor must be exactly 16 bytes (storage* + id + generation)");
    }//namespace ecs
}//namespace hgl
