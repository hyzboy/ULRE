#pragma once

#include<hgl/ecs/core/EntityHandle.h>
// 组件自己的头在这里特化 `ComponentTypeOf<T>`（"类型知道自己占哪个槽"）⇒ 行头带上类型表，
// 与历史 `Component.h` 的连带包含一致（组件头不必各自重复包含）。
#include<hgl/ecs/support/ComponentTypeTable.h>

namespace hgl
{
    namespace ecs
    {
        class Entity;

        /**
         * ComponentRow —— 组件的**行头**（T2-c）：宿主元数据，**不是"基类设施"**。
         *
         * 历史 `Component` 基类实测 **104 B**，而 `GeometryData` 的真实载荷只有 16 B
         * （探针 [T0b]−[T0] = +195.4 B / +3.000 块，其中 95% 是壳）。四样东西各有去处：
         *
         * | 旧形态                                       | T2-c 去处                                                     |
         * |----------------------------------------------|---------------------------------------------------------------|
         * | `std::string componentName`                  | **删除**（只喂调试日志；相机自己保留一个名字，见 `CameraComponent`） |
         * | 虚表（`OnAttach/OnDetach/GetSystemGroup`）    | **删除**：系统组 = 类型属性（`kComponentTypeTable[].group`）；卸载钩子走类型化池（无虚表） |
         * | `enable_shared_from_this`                    | **删除**（组件对象住类型化池：地址稳定、无控制块）；相机例外（见 `SharedComponentPool`） |
         * | `component_slot` / `version` / `change_mask` | **删除**：槽 = 实体侧 `component_rows[]` 的下标；版本/变更位只在真正用它的类型自己身上 |
         *
         * 只留 **8 B**：宿主实体裸指针。实体住块池、地址稳定（T2-a 的结构性不变量：全仓持
         * `Entity*` 永不作废）⇒ 与全仓同纪律；世界经 `owner->GetContext()` 取，不再各存一份世界指针。
         * 行的生存期 ⊂ 宿主的生存期（`~Entity` 先销掉自己全部组件行）⇒ 不会悬垂。
         */
        struct ComponentRow
        {
            Entity *owner = nullptr;

            void SetOwner(Entity *entity) { owner = entity; }

            Entity *GetOwner() const { return owner; }

            /// 宿主实体 ID（无宿主 ⇒ 无效 ID）；实现在 .cpp（需要完整的 `Entity`）
            EntityID GetOwnerID() const;
        };
    }//namespace ecs
}//namespace hgl
