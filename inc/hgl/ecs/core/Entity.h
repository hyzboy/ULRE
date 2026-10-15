#pragma once

#include<hgl/ecs/core/Object.h>
#include<hgl/ecs/core/ComponentRow.h>
#include<hgl/ecs/core/EntityHandle.h>
#include<hgl/ecs/support/ComponentTypeTable.h>
#include<cstdint>

namespace hgl
{
    namespace ecs
    {
        class ECSContext;

        /**
         * Entity - represents game objects with components
         * Entities are containers for components
         *
         * T2-c：**实体槽 = 按类型行号**。组件本体住世界的**类型化对象池**，实体只持
         * `component_rows[ComponentType]`（1 基行号，0 = 无）——与 stage B `EntityGPU::row[16]` 同形
         * （CPU 侧按当前类型数定尺寸）。实体侧因此**零堆分配**（旧形态每挂一个组件都要长一个
         * `vector<ComponentEntry>`），也再没有 `typeid(T).hash_code()` 查表与 `shared_ptr` 控制块。
         *
         * ⚠ 组件的生命周期归**世界**（类型化池）⇒ `AddComponent<T>` 要求实体已挂在世界上
         * （`ECSContext::CreateEntity` 建的实体都满足；栈上裸建的 `Entity` 不能挂组件）。
         */
        class Entity : public Object
        {
        private:

            EntityID id;

            /// **按类型的组件行号表**（T2-c）：下标 = `ComponentType` 值，值 = 该类型在世界类型化池里的
            /// 行号（**1 基**；0 = 无）。这是"本实体有哪些组件、各自在哪一行"的**唯一真源**。
            ///
            /// A1(b)（2026-10-14）：原先并存的**掩码缓存字段已删** —— 同一事实不再存两份（字段与行表
            /// 的失步原需要一条对拍器兜住，现在没有第二份可失步）。需要位图的消费者用
            /// `BuildComponentMask()` **现场拼**（O(COMPONENT_TYPE_COUNT)）。
            uint32_t component_rows[COMPONENT_TYPE_COUNT] = {};

            ECSContext *context = nullptr;   ///< 所属的 ECSContext，不拥有

        public:

            explicit Entity(const char* name) : Object(name) {}
            explicit Entity(const AnsiString& name = "Entity");
            explicit Entity(const std::string& name) : Object(name) {}
            ~Entity() override;

            /// Get entity ID
            // GetEntityID：消除对 Object::GetID（对象 ID）的隐藏——EntityID
            // 与 Object 的 ID 是两套体系，同名隐藏易混淆
            EntityID GetEntityID() const { return id; }

            /// **由行表现场拼**槽位位掩码（O(COMPONENT_TYPE_COUNT)）—— 掩码不再是字段（A1(b) 2026-10-14：
            /// 同一事实不存两份）。生产读者 = 策略路径（`EvaluateRenderNeed` 的输入），每实体每次求值拼一次。
            uint32_t BuildComponentMask() const
            {
                uint32_t mask = 0;

                for (uint32_t i = 1; i < COMPONENT_TYPE_COUNT; ++i)
                    if (component_rows[i])
                        mask |= ComponentTypeBit(static_cast<ComponentType>(i));

                return mask;
            }

            /// 「有没有挂某槽位」问 `HasComponent<T>()`（`Context.h`，只看行号表）或
            /// `GetComponentRow(type) != 0`；无类型号（`None`）恒假 —— 两条都不经掩码。

            /// Set entity ID (called by EntityManager)
            void SetID(EntityID entity_id) { id = entity_id; }

            void SetContext(ECSContext *ctx) { context = ctx; }

            ECSContext *GetContext() const { return context; }

            /// 场景结构（实体/组件增删）变更通知 —— 挂载/卸载路径的唯一副作用，渲染图据此重 gather
            void MarkSceneDirty() const;

        public: // ── 组件行号表（唯一真源；写者是世界的挂载/卸载路径）──

            /// 该类型组件的行号（0 = 没有）
            uint32_t GetComponentRow(ComponentType type) const
            {
                const uint32_t index = static_cast<uint32_t>(type);

                return index < COMPONENT_TYPE_COUNT ? component_rows[index] : 0u;
            }

            /// 写行号（**唯一写者 = 世界的挂载/卸载路径**；本表是唯一真源），返回旧行号
            uint32_t SetComponentRow(ComponentType type, uint32_t row);

        public: // ── 组件挂载/卸载（实现在 Context.h —— 需要完整的 ECSContext）──

            /// 挂载组件（已有同类型 ⇒ 先卸旧行再挂新行，语义与旧 `ReplaceComponent` 逐字一致）。
            /// 返回池内行地址（**裸指针、地址稳定**：池块不移动对象，与 `Entity*` 同纪律）。
            template<typename T, typename... Args> T    *AddComponent(Args &&...args);

            /// 取组件（无 ⇒ nullptr）
            template<typename T>                    T    *GetComponent() const;

            /// 是否挂了该类型组件（只看行号表，O(1)）
            template<typename T>                    bool  HasComponent() const;

            /// 卸载该类型组件（无 ⇒ no-op）
            template<typename T>                    void  RemoveComponent();

            /// 卸载本实体的全部组件（实体销毁 / 清空路径）
            void DetachAllComponents();
        };
    }//namespace ecs
}//namespace hgl
