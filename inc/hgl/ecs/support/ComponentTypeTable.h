/**
 * ComponentTypeTable.h —— 组件类型 → (scope, arena, GPU 行宽) 的**静态表**。
 *
 * 设计依据：doc/future/ULRE_FINAL_TARGET_v2_设计约束.md
 *   §1 Component Data —— "每 (scope, 组件类型) 一个定长行 arena（= 一个 SSBO）"，**scope 静态**
 *                        （组件类型决定作用域，运行时不变）。
 *   §3 Entity        —— GPU 可见组件 = Transform / MaterialData / MaterialRuntime / Geometry / Texture。
 *
 * 约定：
 *   · **新增组件类型时只改这一处**（加枚举项 + 加表项）；表与枚举**同序**由下面的 static_assert 强制，
 *     漏加表项是**编译错误**，不是运行期惊喜。
 *   · `gpu_row_bytes` 是 **GPU 行宽**（定长、符合 GPU 最小对齐）；0 表示该类还没有 GPU 行。
 *     CPU 侧行宽不在这里 —— 它是各存储自己列出的（见 TransformDataStorage::GetPerRowFields）。
 */
#pragma once

#include<cstdint>

namespace hgl
{
    namespace ecs
    {
        /**
         * 组件类型（GPU 可见的那几类；CPU-only 组件如音频/剧情不进 Entity，见 v2 §3）。
         */
        enum class ComponentType : uint8_t
        {
            None = 0,

            Transform,          ///< 变换：每实体一行；L2W 由 CPU 权威 TRS 求值（GPU 行是派生视图）
            Geometry,           ///< 几何体（引用几何数据行）
            MaterialData,       ///< 材质数据（配方/参数，资源级）
            MaterialRuntime,    ///< 材质运行期数据（渲染侧创建/销毁）
            Texture,            ///< 贴图引用
                                ///< **无实体级载体**（B6 判据；零消费者）：纹理引用属于 `MaterialPrivateData`
                                ///<  同族数据（按定义变长），只会出现在「材质定义级行空间」里 ⇒ 与欠账 §2.9 同批处理。
            Camera,             ///< 相机（**世界私有**：CPU 权威数据在 `CameraComponent` 自己身上，
                                ///<  世界那张「16 槽 × 帧槽」相机表是 `CameraSystem::Publish` 出来的**派生视图**）

            Lines,              ///< 线条（**CPU-only 类型号**：T2-c 起实体侧有号，但 gpu_row_bytes = 0、
                                ///<  不建行空间、不写 `EntityGPU.row[]`；与「文本/线条属动态 GEOMETRY，
                                ///<  以后再说」的定界一致 —— 只登记类型号，不做 GPU 建模）
            Text,               ///< 文本（同上：CPU-only 类型号）
            ShadowProxy,        ///< 阴影代理（同上：CPU-only 类型号）
            End,
        };

        inline constexpr uint32_t COMPONENT_TYPE_COUNT = static_cast<uint32_t>(ComponentType::End);

        /**
         * 组件数据所在作用域。**静态**：由组件类型决定，永不变（v2 §1）。
         */
        enum class ComponentScope : uint8_t
        {
            Global = 0,     ///< 跨世界共享（资源级：几何 / 贴图 / 材质数据）
            World,          ///< 世界私有（实例级：变换 / 运行期状态）
        };

        /**
         * 系统组标识（键域 = 组件驱动路径的三个组；T2-c 起它是**类型属性**）。
         *
         * 历史：组件用 `virtual SystemGroupID GetSystemGroup()` 自己声明组名 ⇒ 每个组件对象带一个
         * vtable。T2-c 拆掉虚表 ⇒ 组归属入类型表（`ComponentTypeInfo::group`）：同一个信息只有一个名字，
         * 且与"组件类型"这个静态属性天然同源。
         */
        enum class SystemGroupID : uint8_t
        {
            None = 0,
            Primitive,      ///< 图元（渲染元素标记 = `GeometryData`，A5b）
            Line,           ///< 线条
            Text,           ///< 文本

            Count
        };

        constexpr uint32_t kSystemGroupCount = static_cast<uint32_t>(SystemGroupID::Count);

        struct ComponentTypeInfo
        {
            ComponentType  type;
            ComponentScope scope;
            const char    *arena_name;      ///< 该 (scope, 类型) 的 arena / SSBO 名（注册与日志用）
            uint32_t       gpu_row_bytes;   ///< GPU 行宽（定长、GPU 最小对齐）；0 = 暂无 GPU 行
            const char    *cpu_type_name;   ///< C++ 侧承载类型名（调试用）
            SystemGroupID  group;           ///< 组件挂载驱动哪个系统组（`None` = 不驱动）
        };

        /**
         * 静态表本体。**新增组件类型时只改这里**。
         *
         * scope/行宽已随各类型 ID 化**定稿**（B3 Geometry / B4 MaterialData / B5 MaterialRuntime / B6 Texture）：
         *   **Global** = 跨世界共享的资源级（`Geometry` / `MaterialData` / `Texture`）；
         *   **World** = 世界私有的实例级（`Transform` / `MaterialRuntime`）。
         * `gpu_row_bytes` 只记**已有定长 GPU 行**者（Geometry 的数据在 MeshDrawParams / GeometryAABB 行、
         * `MaterialRuntimeSlot` 是 CPU 侧行 ⇒ 记 0）；改行宽须与 GLSL 侧结构一并核对
         *（并过 S.global-addresses-struct-parity 门）。
         */
        inline constexpr ComponentTypeInfo kComponentTypeTable[COMPONENT_TYPE_COUNT] =
        {
            // 表按枚举值下标索引 ⇒ None=0 必须占位（无 arena/无行宽/不驱动系统组）
            { ComponentType::None,            ComponentScope::World,  "",                     0, "",                    SystemGroupID::None },
            { ComponentType::Transform,       ComponentScope::World,  "ECS:Transform",       64, "TransformDataStorage", SystemGroupID::None },
            { ComponentType::Geometry,        ComponentScope::Global, "ECS:Geometry",         0, "GeometryData",        SystemGroupID::Primitive },   ///< B3：ID 化完成（几何数据落 MeshDrawParams / GeometryAABB 行）。A5b：本组件是"该实体是一个 Primitive 渲染元素"的**唯一标记** ⇒ 驱动图元组；T2-c 起组归属由**类型表**声明（组件虚函数已删）
            { ComponentType::MaterialData,    ComponentScope::Global, "ECS:MaterialData",    16, "MaterialData",        SystemGroupID::None },        ///< B4：组件行 16 B（(Global, MaterialData) arena，行内只放 POD）
            { ComponentType::MaterialRuntime, ComponentScope::World,  "ECS:MaterialRuntime",  0, "MaterialRuntimeSlot", SystemGroupID::None },        ///< B5：slot 全 POD、行宽冻结 88 B（CPU 侧行 ⇒ gpu_row_bytes 记 0）
            { ComponentType::Texture,         ComponentScope::Global, "ECS:Texture",          0, "",                    SystemGroupID::None },        ///< B6：**无组件载体**（纹理授权在 MaterialData 的具名纹理资源里，零消费者，仅登记）；载体形态 = 材质私有数据一族（按定义变长的行空间）⇒ 与欠账 §2.9 同批处理，**勿补实体级组件**
            { ComponentType::Camera,          ComponentScope::World,  "ECS:Camera",           0, "CameraComponent",     SystemGroupID::None },        ///< B7：相机（P-6 ②：**不新开 arena**，参数留组件；世界相机表 = 派生视图 ⇒ 本类型无自己的 GPU 行）；T2-c：相机行内持 `shared_ptr`（槽账目按宿主弱引用计占用期，见 `SharedComponentPool`）
            { ComponentType::Lines,           ComponentScope::World,  "ECS:Lines",            0, "LinesComponent",      SystemGroupID::Line },        ///< T2-c：CPU-only（无 GPU 行；实体侧有类型号）——全场景收集 + 末次一次绘制，收集语义不随存储统一而变
            { ComponentType::Text,            ComponentScope::World,  "ECS:Text",             0, "TextComponent",       SystemGroupID::Text },        ///< T2-c：CPU-only（无 GPU 行；实体侧有类型号）——2D 专用，未来 2D 单开一套管理 ⇒ 现在不得特化
            { ComponentType::ShadowProxy,     ComponentScope::World,  "ECS:ShadowProxy",      0, "ShadowProxy",         SystemGroupID::None },        ///< T2-c：CPU-only（无 GPU 行；实体侧有类型号）
        };

        namespace detail
        {
            /// 表与枚举同序、且每类都被登记（漏一项 ⇒ 编译失败，而不是运行期才发现）
            constexpr bool CheckComponentTypeTable()
            {
                for (uint32_t i = 0; i < COMPONENT_TYPE_COUNT; ++i)
                    if (static_cast<uint32_t>(kComponentTypeTable[i].type) != i)
                        return false;

                return true;
            }
        }

        static_assert(detail::CheckComponentTypeTable(),
                      "组件类型表必须与 ComponentType 枚举同序：新增类型时请在 kComponentTypeTable 同步加表项");

        inline constexpr const ComponentTypeInfo *GetComponentTypeInfo(const ComponentType type)
        {
            const auto index = static_cast<uint32_t>(type);

            return (index < COMPONENT_TYPE_COUNT) ? &kComponentTypeTable[index] : nullptr;
        }

        inline constexpr bool IsValidComponentType(const ComponentType type)
        {
            return (GetComponentTypeInfo(type) != nullptr) && (type != ComponentType::None);
        }

        /// 该类型的 GPU 行宽（预算与 SSBO 布局用）
        inline constexpr uint32_t GetComponentGPURowBytes(const ComponentType type)
        {
            const auto *info = GetComponentTypeInfo(type);

            return info ? info->gpu_row_bytes : 0;
        }

        inline constexpr ComponentScope GetComponentScope(const ComponentType type)
        {
            const auto *info = GetComponentTypeInfo(type);

            return info ? info->scope : ComponentScope::World;
        }

        // ─────────────────────────────────────────────────────────────
        // 位掩码：一个 uint32_t 表达"实体有哪些组件槽位"（stage A 地基；stage B 的 EntityGPU::type[16] 前身）
        // ─────────────────────────────────────────────────────────────

        /// 槽位对应的位；`None`/越界一律返回 0 ⇒ 永远不占位，`HasComponentType(None)` 恒假
        inline constexpr uint32_t ComponentTypeBit(const ComponentType type)
        {
            if (!IsValidComponentType(type))
                return 0;

            return 1u << static_cast<uint32_t>(type);
        }

        inline constexpr bool ComponentMaskHas(const uint32_t mask, const ComponentType type)
        {
            const uint32_t bit = ComponentTypeBit(type);

            return bit != 0 && (mask & bit) != 0;
        }

        // 2026-10-10（审计 §1.1）：`ComponentMaskAdd` / `ComponentMaskRemove` 全仓零调用者 ⇒ 删除
        //（掩码拼装直接写 `| ComponentTypeBit(...)` / `& ~ComponentTypeBit(...)`）。

        // ─────────────────────────────────────────────────────────────
        // implies（蕴含）规则：某类型出现 ⇒ 必然同时具备哪些槽位
        //   · 例：`MaterialRuntime ⇒ MaterialData`（运行期数据必以数据层为源）
        //   · 表只写**直接**蕴含；传递闭包由 `ExpandComponentImplies` 在编译期算完并自检
        // ─────────────────────────────────────────────────────────────

        inline constexpr uint32_t kComponentTypeImplies[COMPONENT_TYPE_COUNT] =
        {
            /* None            */ 0,
            /* Transform       */ 0,
            /* Geometry        */ 0,
            /* MaterialData    */ 0,
            /* MaterialRuntime */ ComponentTypeBit(ComponentType::MaterialData),
            /* Texture         */ 0,
            /* Camera          */ 0,
            /* Lines           */ 0,
            /* Text            */ 0,
            /* ShadowProxy     */ 0,
        };

        /// implies 掩码的**不动点展开**：把掩码里每个槽位的蕴含并进来，反复迭代到不变。
        /// 组件种类是个位数，代价可忽略；只在编译期与规则求值用。
        ///
        /// **单一实现**：`GetImpliedComponentMask`（单类型入口）与 `RenderStrategyTable` 的
        /// `EvaluateRenderNeed`（多槽掩码入口）此前各自内联了同一条迭代循环 ⇒ 收成这一份。
        /// 多槽入口的正确性依据：F(A) = A ∪ ⋃_{i∈A} implies[i] 对并集可加（F(A∪B) = F(A) ∪ F(B)）
        /// ⇒ 任意掩码的闭包 == 各位闭包之并（下面的 `CheckComponentMaskClosure` 对全部
        /// 单点与两两组合自检：同源 / 幂等 / 可加）。
        constexpr uint32_t ExpandComponentImplies(uint32_t slots)
        {
            for (uint32_t round = 0; round <= COMPONENT_TYPE_COUNT; ++round)
            {
                const uint32_t before = slots;

                for (uint32_t i = 0; i < COMPONENT_TYPE_COUNT; ++i)
                    if (slots & (1u << i))
                        slots |= kComponentTypeImplies[i];

                if (slots == before)
                    break;
            }

            return slots;
        }

        /// 该类型的**完整蕴含掩码**（含自身位）；编译期展开传递闭包
        constexpr uint32_t GetImpliedComponentMask(const ComponentType type)
        {
            const uint32_t self = ComponentTypeBit(type);

            if (self == 0)
                return 0;

            return ExpandComponentImplies(self);
        }

        namespace detail
        {
            /// 自检（自动 scale：遍历全表，新增类型自动纳入）：
            ///   ① 蕴含掩码含自身位；② 闭包幂等（每个成员的闭包都被原闭包包含 ⇒ 已闭包）
            constexpr bool CheckComponentTypeImplies()
            {
                for (uint32_t i = 0; i < COMPONENT_TYPE_COUNT; ++i)
                {
                    const ComponentType type = static_cast<ComponentType>(i);

                    if (type == ComponentType::None)
                        continue;

                    const uint32_t mask = GetImpliedComponentMask(type);

                    if ((mask & ComponentTypeBit(type)) == 0)
                        return false;

                    for (uint32_t j = 0; j < COMPONENT_TYPE_COUNT; ++j)
                    {
                        if ((mask & (1u << j)) == 0)
                            continue;

                        if ((GetImpliedComponentMask(static_cast<ComponentType>(j)) & ~mask) != 0)
                            return false;
                    }
                }

                return true;
            }

            /// 自检（自动 scale：遍历全表，新增类型自动纳入）：
            ///   ① 多槽入口与单类型入口**同源**（Expand(自身位) == GetImpliedComponentMask）；
            ///   ② 幂等（闭包再闭包不变）；③ 可加（两两组合的闭包 == 各自闭包之并）。
            /// 两条路径漂移**不会报错**，只会让规则表静默不一致 —— 这条断言就是防漂移的门。
            constexpr bool CheckComponentMaskClosure()
            {
                if (ExpandComponentImplies(0) != 0)
                    return false;

                for (uint32_t i = 0; i < COMPONENT_TYPE_COUNT; ++i)
                {
                    const ComponentType ti    = static_cast<ComponentType>(i);
                    const uint32_t      bit_i = ComponentTypeBit(ti);
                    const uint32_t      once  = ExpandComponentImplies(bit_i);

                    if (once != GetImpliedComponentMask(ti))
                        return false;

                    if (ExpandComponentImplies(once) != once)
                        return false;

                    for (uint32_t j = i + 1; j < COMPONENT_TYPE_COUNT; ++j)
                    {
                        const uint32_t bit_j = ComponentTypeBit(static_cast<ComponentType>(j));
                        const uint32_t both  = ExpandComponentImplies(bit_i | bit_j);

                        if (both != (once | ExpandComponentImplies(bit_j)))
                            return false;

                        if (ExpandComponentImplies(both) != both)
                            return false;
                    }
                }

                return true;
            }
        }

        static_assert(detail::CheckComponentMaskClosure(),
                      "implies 掩码闭包自检失败（多槽入口与单类型入口漂移 / 幂等或可加性不成立）");

        static_assert(detail::CheckComponentTypeImplies(),
                      "组件类型 implies 规则不自洽：必须自反（含自身位）且传递闭包幂等（检查是否成环或漏项）");

        // ─────────────────────────────────────────────────────────────
        // 类型 → 槽位（编译期映射）
        //   特化写在各自的组件头里（"类型知道自己占哪个槽"），这样 `Entity::AddComponent<T>`
        //   编译期即可拿到槽位；**没有特化的类型不占槽位**（`None`）⇒ T2-c 起
        //   `AddComponent<T>` 对 `None` 类型是**编译期错误**（组件必须有类型号：实体侧按类型存行号，
        //   槽 = 实体句柄表的下标）。历史上相机/线条/文本都没有号，这正是 T2-c 前置① 补上的原因。
        // ─────────────────────────────────────────────────────────────

        template<typename T>
        struct ComponentTypeOf
        {
            static constexpr ComponentType value = ComponentType::None;
        };

        /// 该类型挂载时驱动哪个系统组（类型属性；`None` = 不驱动）
        inline constexpr SystemGroupID GetComponentGroup(const ComponentType type)
        {
            const auto *info = GetComponentTypeInfo(type);

            return info ? info->group : SystemGroupID::None;
        }

        // ──────────────────────────────────────────────────────
        // 系统组（P-4 拍板 2026-10-02；T2-c 起 = **类型属性**，见 `ComponentTypeInfo::group`）
        //
        // 「组件挂卸 ⇒ 装/启系统组」从**按组名字符串**收敛为**枚举 + O(1) 数组**（热路径无字符串哈希）；
        // T2-c 拆掉组件虚表后，组归属也离开了组件对象（`virtual GetSystemGroup()` 已删）⇒ 它是
        // **类型**的属性，住在 `kComponentTypeTable[].group`（同一个信息只有一个名字）。
        //
        // ⚠ 系统侧（`System::SetRenderElementType("…")`，14 个系统）仍是字符串声明 —— 那是
        //   **一次性注册路径**（组定义 / 安装器 / 管线），不在本映射范围；两侧同名由 Test 25 契约钉住。
        // ──────────────────────────────────────────────────────

        /// 组名（**唯一一份字符串**）：只用于桥接仍按名键控的一次性路径（组注册表 / 安装器 / 管线）。
        /// ⚠ 组增删只需改 `SystemGroupID`（本数组与各处 `[kSystemGroupCount]` 数组都随枚举自动 scale），
        ///   但**系统侧的 `SetRenderElementType` 字符串**是另一套声明，增删时必须同步（Test 25 契约钉住）。
        constexpr const char *kSystemGroupNames[kSystemGroupCount] =
        {
            "",             // None
            "Primitive",
            "Line",
            "Text",
        };

        constexpr bool SystemGroupNameEquals(const char *a, const char *b) noexcept
        {
            if (!a || !b)
                return false;

            for (uint32_t i = 0;; ++i)
            {
                if (a[i] != b[i])
                    return false;

                if (a[i] == '\0')
                    return true;
            }
        }

        /// 组名 → `SystemGroupID`（严格：只认 `kSystemGroupNames`；未知返回 `None`）
        constexpr SystemGroupID SystemGroupFromName(const char *name) noexcept
        {
            for (uint32_t g = 0; g < kSystemGroupCount; ++g)
                if (SystemGroupNameEquals(name, kSystemGroupNames[g]))
                    return static_cast<SystemGroupID>(g);

            return SystemGroupID::None;
        }
    }//namespace ecs
}//namespace hgl
