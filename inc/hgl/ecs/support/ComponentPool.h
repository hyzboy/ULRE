#pragma once

#include<hgl/ecs/core/ComponentRow.h>
#include<hgl/ecs/support/ComponentTypeTable.h>
#include<algorithm>
#include<cstddef>
#include<cstdint>
#include<memory>
#include<new>
#include<type_traits>
#include<utility>
#include<vector>

namespace hgl
{
    namespace ecs
    {
        class Entity;

        /**
         * ComponentPoolBase —— 「一个类型的组件存储」的**世界侧统一接口**（T2-c）。
         *
         * 虚函数只落在**池**上（每世界每类型至多一个实例，全仓 ≤6 个），**不在组件行上**：
         * 组件对象里没有 vptr —— 这正是 T2-c 的主要字节项（`Component` 基类 104 B 中
         * 虚表 + 名字字符串占绝大多数，`GeometryData` 的真实载荷只有 16 B）。
         */
        class ComponentPoolBase
        {
        public:

            virtual ~ComponentPoolBase() = default;

            virtual ComponentType GetComponentType() const = 0;

            /// 活跃行数 —— 活跃集**遍历**的通用入口。
            /// （`component_pool_slots[]` 是基类指针数组，此前基类只有 3 个虚函数、拿不到计数
            ///   ⇒ 任何按类型遍历活跃集的调用点都只能 static_cast 回具体类型。）
            virtual uint32_t GetActiveCount() const = 0;

            /// 释放一行（显式析构 + 行归还空闲表）；`handle == 0` / 越界 / 已释放 ⇒ no-op
            virtual void DestroyRow(uint32_t handle) = 0;

            /// 释放全部行（世界 Shutdown / Clear；块保留复用至池析构）
            virtual void Clear() = 0;
        };

        /**
         * ComponentRowLedger —— 池的**行账目**：空闲表 + 活跃集（尾替 O(1) 摘除）+ 已分配行数。
         *
         * `ComponentPool<T>`（块池）与 `SharedComponentPool<T>`（shared_ptr 行）只在
         * 「行的内存怎么来、怎么还」上不同：账目这套此前被逐字复制了两遍
         * （`free_rows` / `active_rows` / `active_pos` / `next_index` + `RemoveActive`
         * + 活跃集查询 + `Clear` 的四阶段复位体），现收到这里供两者**组合**持有。
         *
         * 行号约定与两个池一致：**句柄 = 下标 + 1**（`0` = 无），`kNoPos` = 非活跃。
         */
        class ComponentRowLedger
        {
        public:

            static constexpr uint32_t kNoPos = UINT32_MAX;

        private:

            std::vector<uint32_t> free_rows;        ///< 空闲行（0 基下标，LIFO）
            std::vector<uint32_t> active_rows;      ///< 活跃集（0 基下标；尾替删除，顺序无关）
            std::vector<uint32_t> active_pos;       ///< 行 → 活跃集位置（O(1) 摘除；kNoPos = 空洞）
            uint32_t              next_index = 0;   ///< 已分配过的行数（0 基上界）

        public:

            uint32_t GetNextIndex() const { return next_index; }

            /// 取一个可用下标：优先空闲表，否则追加（同时把 `active_pos` 扩到 `next_index`）
            uint32_t AllocateIndex()
            {
                if (!free_rows.empty())
                {
                    const uint32_t index = free_rows.back();
                    free_rows.pop_back();
                    return index;
                }

                const uint32_t index = next_index++;

                if (active_pos.size() < next_index)
                    active_pos.resize(next_index, kNoPos);

                return index;
            }

            /// 认领该下标为活跃（`Create` 的最后一步）
            void MarkActive(uint32_t index)
            {
                active_pos[index] = uint32_t(active_rows.size());
                active_rows.push_back(index);
            }

            bool IsActive(uint32_t index) const
            {
                return index < active_pos.size() && active_pos[index] != kNoPos;
            }

        private:

            /// 摘除活跃（尾替 + 回填；非活跃 ⇒ no-op）—— 只由 `ReleaseIndex` 调用，不对外
            void RemoveActive(uint32_t index)
            {
                if (index >= active_pos.size())
                    return;

                const uint32_t pos = active_pos[index];

                if (pos == kNoPos)
                    return;

                const uint32_t last = active_rows.back();

                active_rows[pos]  = last;
                active_pos[last]  = pos;
                active_rows.pop_back();
                active_pos[index] = kNoPos;
            }

        public:

            /// 归还下标（摘除活跃 + 压入空闲表）
            void ReleaseIndex(uint32_t index)
            {
                RemoveActive(index);
                free_rows.push_back(index);
            }

            /// 全部复位：清活跃集；空闲表重新铺满 `[0, next_index)`（四阶段体，与改造前同序）
            void ResetAll()
            {
                active_rows.clear();

                if (!active_pos.empty())
                    std::fill(active_pos.begin(), active_pos.end(), kNoPos);

                free_rows.clear();
                free_rows.reserve(next_index);

                for (uint32_t index = 0; index < next_index; ++index)
                    free_rows.push_back(index);
            }

        public: // 活跃集视图（收集路径的遍历源；不分配，不拷贝）

            uint32_t GetActiveCount() const { return uint32_t(active_rows.size()); }

            /// 活跃集第 `i` 项的**行下标**（0 基；调用方自己取行指针）
            uint32_t GetActiveIndex(uint32_t i) const { return active_rows[i]; }
        };

        /**
         * ComponentPool<T> —— **类型化对象池**（T2-c「取消逐 new」的落点）。
         *
         * 手法与 `EntityManager` 的实体块池同源：
         *   · **块式**（1 块 = 64 行）就地 placement-new ⇒ 行地址**稳定**，可长期持裸指针
         *     （与全仓持 `Entity*` 同纪律）；每 64 个组件才付一次堆分配（摊薄 ≈ 0.016 块/组件）。
         *   · **池即活跃集**：`active_rows` 是紧凑的遍历源（收集路径只读它，除调用方容器外零分配），
         *     `active_pos` 让摘除保持 O(1)（尾替 + 回填）。
         *   · 行号 **1 基**（`0` = 无，与全仓"行 0 哨兵"惯例一致）⇒ 实体侧
         *     `component_rows[type] == 0` 即"本实体没有这个组件"。
         *   · 行内宿主元数据 = `ComponentRow`（**只 8 B**：宿主实体指针），挂载时由池写入
         *     ⇒ 组件不再自己 `SetOwner`，也不需要"我属于谁"的第二个名字。
         *
         * 消掉的东西（旧形态 = 派生自 `Component` + `std::make_shared`）：
         *   ① 每个组件两次堆分配（对象 + 控制块，实测 3.000 块/组件）
         *   ② 基类的名字字符串与虚表（类型属性搬去类型表）
         *   ③ 实体侧 `vector<ComponentEntry>` 的逐实体分配。
         */
        template<typename T>
        class ComponentPool final : public ComponentPoolBase
        {
            static_assert(std::is_base_of_v<ComponentRow, T>,
                          "组件类型必须派生自 ComponentRow（T2-c 的行头：宿主元数据）");

            static_assert(alignof(T) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__,
                          "组件行对齐超过默认 new 对齐：块池需改用 aligned new（ComponentPool）");

        public:

            static constexpr uint32_t kBlockRows = 64;

        private:

            std::vector<std::unique_ptr<std::byte[]>> blocks;   ///< 行块（地址稳定，随池析构释放）
            ComponentRowLedger ledger;                          ///< 空闲表 / 活跃集（与 SharedComponentPool 同一份账目）

        private:

            T *RowAt(uint32_t index) const
            {
                return reinterpret_cast<T *>(blocks[index / kBlockRows].get() + (index % kBlockRows) * sizeof(T));
            }

            void EnsureBlockFor(uint32_t index)
            {
                const uint32_t block_index = index / kBlockRows;

                while (blocks.size() <= block_index)
                    blocks.emplace_back(std::make_unique<std::byte[]>(size_t(kBlockRows) * sizeof(T)));
            }

            /// 析构一行（卸载钩子 → 显式析构）；**不做账目维护**（调用方按需 `ReleaseIndex`）
            void DestroyRowAt(uint32_t index)
            {
                T *row = RowAt(index);

                // 卸载钩子：类型自己声明（`OnDetach()`）；无虚表 ⇒ 编译期判定，没有该类钩子就是零代码
                if constexpr (requires(T &r) { r.OnDetach(); })
                    row->OnDetach();

                // 2026-10-11（§1 S2）：**先摘宿主再析构**。原顺序在 `row->~T()` **之后**写
                // `SetOwner(nullptr)` —— 那是对**生命周期已结束**的对象写其基类子对象（UB；
                // 只因行头恰好是 POD、内存未归还才"没炸"）。顺序反过来才是正确语义。
                static_cast<ComponentRow *>(row)->SetOwner(nullptr);

                row->~T();
            }

        public:

            ~ComponentPool() override
            {
                Clear();        // 显式析构全部行（块随成员析构释放）
            }

        public:

            ComponentType GetComponentType() const override { return ComponentTypeOf<T>::value; }
            uint32_t      GetActiveCount()   const override { return ledger.GetActiveCount(); }

            /// 句柄 → 行指针（`0` / 越界 / **已释放** ⇒ nullptr）
            T *Row(uint32_t handle) const
            {
                if (handle == 0)
                    return nullptr;

                const uint32_t index = handle - 1;

                if (index >= ledger.GetNextIndex())
                    return nullptr;

                // 2026-10-11（§1 S2）：必须验活跃。只判上界时，`DestroyRow` 之后这里仍返回
                // 指向**已析构行**的非空指针 —— 与 `ComponentPoolBase::DestroyRow` 的
                // "已释放 ⇒ no-op"、`ComponentRow::GetOwner` 一族的"已释放 ⇒ nullptr"承诺相悖，
                // 也与 `SharedComponentPool<T>::Row`（reset 后自然是 nullptr）语义不一致。
                if (!ledger.IsActive(index))
                    return nullptr;

                return RowAt(index);
            }

            /**
             * 建一行：块内就地构造 + 写入宿主元数据，返回**句柄**（1 基）。
             * 调用方（世界）负责把句柄写进实体槽 —— 那是唯一真源。
             */
            template<typename... Args>
            uint32_t Create(Entity *owner, Args &&...args)
            {
                const uint32_t index = ledger.AllocateIndex();

                EnsureBlockFor(index);

                T *row = RowAt(index);

                new (row) T(std::forward<Args>(args)...);
                static_cast<ComponentRow *>(row)->SetOwner(owner);

                ledger.MarkActive(index);

                return index + 1;
            }

            void DestroyRow(uint32_t handle) override
            {
                if (handle == 0)
                    return;

                const uint32_t index = handle - 1;

                if (index >= ledger.GetNextIndex() || !ledger.IsActive(index))
                    return;     // 空洞（重复释放）⇒ no-op

                DestroyRowAt(index);
                ledger.ReleaseIndex(index);
            }

            void Clear() override
            {
                for (uint32_t index = 0; index < ledger.GetNextIndex(); ++index)
                {
                    if (!ledger.IsActive(index))
                        continue;

                    DestroyRowAt(index);
                }

                ledger.ResetAll();
            }

        public: // 活跃集视图（收集路径的遍历源；不分配，不拷贝）


            T *GetActiveRow(uint32_t i) const { return RowAt(ledger.GetActiveIndex(i)); }
        };

        /**
         * SharedComponentPool<T> —— 行内**持 `shared_ptr`** 的类型化池（T2-c：相机的独立所有者）。
         *
         * 为什么相机不能住上面的裸行池：世界相机槽的账目（`CameraInfoStorage::slot_owners`）是
         * `weak_ptr<CameraComponent>`（"宿主生命期 = 槽占用期"），而认领槽的路径
         * （`ECSContext::EnsureCameraSlot` / `CameraSystem::ClaimDefaultCamera`）取的是
         * `camera->weak_from_this().lock()` ⇒ **宿主必须由 shared_ptr 长期持有**（同一控制块）；
         * 这正是相机作为"世界级资源、实体只是宿主"的既有定案（B7/P-6 ②）。
         *
         * 语义与 `ComponentPool<T>` 逐条一致（行号 1 基、池即活跃集、行地址稳定、宿主元数据在行头）；
         * 差别只有布局：行的所有权是 `shared_ptr`（每相机一次分配；相机 ≤16/世界 ⇒ 与逐实体开销无关）。
         */
        template<typename T>
        class SharedComponentPool final : public ComponentPoolBase
        {
            static_assert(std::is_base_of_v<ComponentRow, T>,
                          "组件类型必须派生自 ComponentRow（T2-c 的行头：宿主元数据）");

        private:

            std::vector<std::shared_ptr<T>> rows;       ///< 行（shared_ptr 持有；下标 = 句柄 - 1）
            ComponentRowLedger              ledger;     ///< 空闲表 / 活跃集（与 ComponentPool 同一份账目）

        private:

            /// 卸载一行（钩子 + 归还行号 + 释放 shared_ptr —— 最后一个持有者析构 ⇒ 槽账目的弱引用同时失效）
            void DestroyRowAt(uint32_t index)
            {
                if constexpr (requires(T &r) { r.OnDetach(); })
                    rows[index]->OnDetach();

                ledger.ReleaseIndex(index);

                rows[index].reset();
            }

        public:

            ~SharedComponentPool() override
            {
                Clear();
            }

            ComponentType GetComponentType() const override { return ComponentTypeOf<T>::value; }
            uint32_t      GetActiveCount()   const override { return ledger.GetActiveCount(); }

            T *Row(uint32_t handle) const
            {
                if (handle == 0)
                    return nullptr;

                const uint32_t index = handle - 1;

                return index < rows.size() ? rows[index].get() : nullptr;
            }

            template<typename... Args>
            uint32_t Create(Entity *owner, Args &&...args)
            {
                const uint32_t index = ledger.AllocateIndex();

                if (rows.size() < ledger.GetNextIndex())
                    rows.resize(ledger.GetNextIndex());

                rows[index] = std::make_shared<T>(std::forward<Args>(args)...);
                static_cast<ComponentRow *>(rows[index].get())->SetOwner(owner);

                ledger.MarkActive(index);

                return index + 1;
            }

            void DestroyRow(uint32_t handle) override
            {
                if (handle == 0)
                    return;

                const uint32_t index = handle - 1;

                if (index >= rows.size() || !ledger.IsActive(index))
                    return;

                DestroyRowAt(index);
            }

            void Clear() override
            {
                for (uint32_t index = 0; index < ledger.GetNextIndex(); ++index)
                {
                    if (!ledger.IsActive(index))
                        continue;

                    if constexpr (requires(T &r) { r.OnDetach(); })
                        rows[index]->OnDetach();

                    rows[index].reset();
                }

                ledger.ResetAll();
            }

        public: // 活跃集视图


            T *GetActiveRow(uint32_t i) const { return rows[ledger.GetActiveIndex(i)].get(); }
        };

        /**
         * 组件 → 池形态。默认：裸行池。**需要 `shared_ptr` 宿主的类型在自己头里改指
         * `SharedComponentPool`**（当前只有 `CameraComponent`：它的槽账目按宿主弱引用计占用期）。
         */
        template<typename T>
        struct ComponentStorageTraits
        {
            using PoolType = ComponentPool<T>;
        };
    }//namespace ecs
}//namespace hgl
