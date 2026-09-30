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

        struct ComponentTypeInfo
        {
            ComponentType  type;
            ComponentScope scope;
            const char    *arena_name;      ///< 该 (scope, 类型) 的 arena / SSBO 名（注册与日志用）
            uint32_t       gpu_row_bytes;   ///< GPU 行宽（定长、GPU 最小对齐）；0 = 暂无 GPU 行
            const char    *cpu_type_name;   ///< C++ 侧承载类型名（调试用）
        };

        /**
         * 静态表本体。**新增组件类型时只改这里**。
         *
         * ⚠ 后四类的 `scope` 待各自 ID 化时定稿：现记 `World`（世界私有）作**保守默认**；
         *   按资源语义 `Geometry / Texture / MaterialData` 很可能应改 `Global`。
         *   行宽同理 —— 定稿时与 GLSL 侧结构一并核对（并过 S.global-addresses-struct-parity 门）。
         */
        inline constexpr ComponentTypeInfo kComponentTypeTable[COMPONENT_TYPE_COUNT] =
        {
            // 表按枚举值下标索引 ⇒ None=0 必须占位（无 arena/无行宽）
            { ComponentType::None,            ComponentScope::World, "",                     0, "" },
            { ComponentType::Transform,       ComponentScope::World,  "ECS:Transform",       64, "TransformDataStorage" },
            { ComponentType::Geometry,        ComponentScope::Global, "ECS:Geometry",         0, "(待 ID 化)" },
            { ComponentType::MaterialData,    ComponentScope::Global, "ECS:MaterialData",     0, "(待 ID 化)" },
            { ComponentType::MaterialRuntime, ComponentScope::World,  "ECS:MaterialRuntime",  0, "(待 ID 化)" },
            { ComponentType::Texture,         ComponentScope::Global, "ECS:Texture",          0, "(待 ID 化)" },
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

        inline constexpr uint32_t ComponentMaskAdd(const uint32_t mask, const ComponentType type)
        {
            return mask | ComponentTypeBit(type);
        }

        inline constexpr uint32_t ComponentMaskRemove(const uint32_t mask, const ComponentType type)
        {
            return mask & ~ComponentTypeBit(type);
        }

        // ─────────────────────────────────────────────────────────────
        // implies（蕴含）规则：某类型出现 ⇒ 必然同时具备哪些槽位
        //   · 例：`MaterialRuntime ⇒ MaterialData`（运行期数据必以数据层为源）
        //   · 表只写**直接**蕴含；传递闭包由 GetImpliedComponentMask 在编译期算完并自检
        // ─────────────────────────────────────────────────────────────

        inline constexpr uint32_t kComponentTypeImplies[COMPONENT_TYPE_COUNT] =
        {
            /* None            */ 0,
            /* Transform       */ 0,
            /* Geometry        */ 0,
            /* MaterialData    */ 0,
            /* MaterialRuntime */ ComponentTypeBit(ComponentType::MaterialData),
            /* Texture         */ 0,
        };

        /// 该类型的**完整蕴含掩码**（含自身位）；编译期展开传递闭包
        constexpr uint32_t GetImpliedComponentMask(const ComponentType type)
        {
            const uint32_t self = ComponentTypeBit(type);

            if (self == 0)
                return 0;

            uint32_t mask = self;

            // 反复迭代到不动点。类型种类是个位数，代价可忽略。
            for (uint32_t round = 0; round <= COMPONENT_TYPE_COUNT; ++round)
            {
                const uint32_t before = mask;

                for (uint32_t i = 0; i < COMPONENT_TYPE_COUNT; ++i)
                    if (mask & (1u << i))
                        mask |= kComponentTypeImplies[i];

                if (mask == before)
                    break;
            }

            return mask;
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
        }

        static_assert(detail::CheckComponentTypeImplies(),
                      "组件类型 implies 规则不自洽：必须自反（含自身位）且传递闭包幂等（检查是否成环或漏项）");

        // ─────────────────────────────────────────────────────────────
        // 类型 → 槽位（编译期映射）
        //   特化写在各自的组件头里（"类型知道自己占哪个槽"），这样 Entity::AddComponent<T> 编译期即可拿到槽位；
        //   没有特化的类型 ⇒ `None`（不占 Entity 槽位，仍是合法的普通组件，如 Camera/Lines 等 CPU 域组件）。
        // ─────────────────────────────────────────────────────────────

        template<typename T>
        struct ComponentTypeOf
        {
            static constexpr ComponentType value = ComponentType::None;
        };

        template<typename T>
        inline constexpr ComponentType ComponentTypeOf_v = ComponentTypeOf<T>::value;
    }//namespace ecs
}//namespace hgl
