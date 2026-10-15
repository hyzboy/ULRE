#pragma once

#include <cstdint>
#include <cstddef>
#include <hgl/ecs/support/ComponentTypeTable.h>

namespace hgl
{
    namespace ecs
    {
        /**
         * EntityGPU —— 实体的 **GPU 派生视图**（128 B 定长行）。
         *
         * 设计输入（不可擅改，见 `doc/future/entitygpu-and-ulrescene-design.md` §0.1）：
         *   · 形状 = `flags(4) + type[16](16) + row[16](64) + work_flags(4)` = **88 B 载荷**
         *     ⇒ `alignas(64)` ⇒ **128 B**（40 B 填充是该对齐的必然结果，不是随手留的）；
         *   · 槽位语义 = **`ComponentType` 值**、**每类型最多一行**（P-3 拍板：与 `Entity::component_rows[16]` 1:1；
         *     A1(b) 2026-10-14 起实体侧的**掩码缓存字段已删** ⇒ 这里对应的是行表本身）；
         *   · `row[i] == 0` = 无（全仓「行 0 哨兵」惯例）；
         *   · **相机 / 阴影 / 可见性等世界级表不进本结构**（v2 §3 + P-6：相机参数留组件、世界相机表是派生视图）。
         *
         * 归属（设计稿 §2.2）：**世界私有** —— 实体属世界（`EntityManager` 的槽表/空闲栈都在世界内），
         * 落到设备级全局池会在多世界同帧互相覆盖同一批行（相机行当年踩过的同型坑）。
         * 派生表本体与 GLSL 镜像**等第一个真消费者**（GPU-Driven 读实体行 / GPGPU 场景查询）随设计稿的 T3 一起做。
         *
         * 本期状态（T1）：**只有结构与断言 —— 不接线、无字段读写者**。
         * 「0 消费者 = 投机 API」纪律下，这里刻意不提供 Has/Set/Get 一类便捷函数：谁读谁写，随消费者一起加。
         */

        /// 实体级标志位（`flags`）；工作期派生位另见 `work_flags`
        enum class EntityGPUFlags : uint32_t
        {
            None    = 0,
            Active  = 1u << 0,      ///< 槽在用（实体销毁后清位）
            /// ⚠ **扁平派生位**：不继承祖先 —— 只镜像 `SetEntityVisible` 落在**这一个**实体上的值，
            /// 父不可见时后代此行仍可能为 1。消费者**不得仅凭它判定「该画」**：最终闸门 =
            /// 4-ID 表 `geometry_id == 0` 跳过 + 批标签 `0xFFFFFFFF` 跳过。
            /// 权威链（三种表示 / 两种极性 / 传播规则 / 已知失步）见 doc/visibility-authority-chain.md。
            Visible = 1u << 1,
        };

        /// `type` / `row` 的槽数（设计输入：16 槽）
        constexpr uint32_t kEntityGPUSlotCount = 16;

        static_assert(COMPONENT_TYPE_COUNT <= kEntityGPUSlotCount,
                      "组件类型数已超过 EntityGPU 的 16 槽：P-3「每类型一槽」的前提失效，需先扩容设计");

        /// 实体的 GPU 派生视图（128 B 定长行；CPU 权威是 `Entity` + 各类型行号）
        struct alignas(64) EntityGPU
        {
            uint32_t flags = 0;                     ///< `EntityGPUFlags` 位或
            uint8_t  type[kEntityGPUSlotCount] = {}; ///< 槽 i != 0 ⇒ 本实体有 `ComponentType(i)` 的组件
                                                    ///< （槽号已蕴含类型 ⇒ 该数组是**显式冗余**；按 v2 §3 保留，
                                                    ///<   便于着色器判空写法直观：`if (e.type[CT] != 0)`）
            uint32_t row [kEntityGPUSlotCount] = {}; ///< 槽 i 对应的 arena 行号；0 = 无
            uint32_t work_flags = 0;                ///< 工作期派生位（由 type/row 推出）；**留 0**，
                                                    ///< 等消费者定义后再填位表（不造没人读的位）

            // ── T3h：阴影接收事实（形态 B 接管的 GPU 通路，第一个真消费者）──────
            // 原 `_pad` 区划出 8 B：128 B 步长与 alignas(64) 不变（填充是契约、
            // 划用不破坏契约）。写者 = collect 在「本项进批」判据点与 4-ID 表同行
            // 写入（ShadowProxy 是 CPU-only 组件，T2-c；这里放**事实**不放行号——
            // 组件行号对 GPU 无用，值进行才算接线）。
            // 消费规则现由**实体事实 → 剔除写行**两跳承担（原 CPU 批行写入段已随整族删除）；
            // 与 pcf_shadow.glsl 的约定同式：
            //   bias_multiplier <= 0 ⇒ 行里留 0，着色侧视 0 为引擎默认 1.0
            //   receive = 0 ⇒ 消费端写 kMaterialShadowFlagNoReceive
            float  shadow_bias_multiplier = 0.0f;   ///< 局部 Bias 倍率事实（0 = 未上行/默认）
            uint32_t shadow_receive       = 0;      ///< 1 = 接收阴影（CanReceiveShadow 事实位）
        };

        static_assert(sizeof(EntityGPU) == 128,
                      "EntityGPU 必须 128 B：88 B 载荷（4+16+64+4）+ 40 B 填充 = alignas(64) 的必然结果");
        static_assert(alignof(EntityGPU) == 64, "EntityGPU 必须 64 B 对齐（单实体命中缓存行）");
        static_assert(offsetof(EntityGPU, flags)      ==  0, "字段顺序/偏移被改动（GPU 侧镜像会错位）");
        static_assert(offsetof(EntityGPU, type)       ==  4, "字段顺序/偏移被改动");
        static_assert(offsetof(EntityGPU, row)        == 20, "字段顺序/偏移被改动");
        static_assert(offsetof(EntityGPU, work_flags) == 84, "字段顺序/偏移被改动");
        static_assert(offsetof(EntityGPU, shadow_bias_multiplier) == 88,
                      "阴影事实字段偏移被改动（_pad 划用后 GPU 镜像会错位）");
        static_assert(offsetof(EntityGPU, shadow_receive) == 92,
                      "阴影事实字段偏移被改动");
    }//namespace ecs
}//namespace hgl
