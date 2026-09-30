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
            { ComponentType::Transform,       ComponentScope::World, "ECS:Transform",       64, "TransformDataStorage" },
            { ComponentType::Geometry,        ComponentScope::World, "ECS:Geometry",         0, "(未 ID 化)" },
            { ComponentType::MaterialData,    ComponentScope::World, "ECS:MaterialData",     0, "(未 ID 化)" },
            { ComponentType::MaterialRuntime, ComponentScope::World, "ECS:MaterialRuntime",  0, "(未 ID 化)" },
            { ComponentType::Texture,         ComponentScope::World, "ECS:Texture",          0, "(未 ID 化)" },
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
            return GetComponentTypeInfo(type) != nullptr;
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
    }//namespace ecs
}//namespace hgl
