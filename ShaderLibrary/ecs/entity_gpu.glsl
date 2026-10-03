// @ulre begin
// @ulre name entity_gpu
// @ulre kind Utility
// @ulre priority 0
// @ulre end
// entity_gpu.glsl — EntityGPU 实体行的 GLSL 镜像（GPU 剔除的枚举源，T3d-1）
//
// C++ 真源 = inc/hgl/ecs/core/EntityGPU.h（128 B 定长行：flags + type[16] + row[16]
// + work_flags + 40 B 填充，alignas(64)）。两端逐字段同序同型由门用例
// E.entity-gpu-struct-parity 对拍兜底 —— 改任何一端必须同步另一端。
//
// 寻址（§7.2 / P-2）：表基址走世界地址表 world_addresses.addr_entity_gpu（pc_root
// 已指向本帧槽 ⇒ 拿到的恒是**本帧**那份表），行 = 基址 + EntityID.index × 128，
// 直索引、无 CPU 紧凑表。用法：entity_gpu_rows[index].flags / .type[CT] / .row[CT]。
//
// _pad[40] 是**布局契约**不是装饰：scalar layout 下 GLSL 结构按成员紧凑排布，
// 88 B 载荷若不以 _pad 填回 40 B，数组步长就是 88 而非 C++ 端 alignas(64) 的
// 128 ⇒ 第 2 行起全部错位。填充放在行尾（offset 88..127），任何 C++ 字段的偏移
// 都不受影响，与头文件的 offsetof 断言族互不干扰。
//
// 容量 4096（P-B8-6）不进本结构：那是表级常量，由 C++ 端持有（本文件是行形状，
// 不是表边界）。
#ifndef HGL_ENTITY_GPU_GLSL
#define HGL_ENTITY_GPU_GLSL

#include "common/descriptor_macros.glsl"
#include "ubo/scene_ubo.glsl"

#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_ARB_gpu_shader_int64 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable

#define ENTITY_GPU_SLOT_COUNT 16   // C++ 真源：kEntityGPUSlotCount（EntityGPU.h）

// 行镜像：类型名与库内其他表镜像同族（CameraInfoData / ShadowInfoRef …）
struct EntityGPUData
{
    uint flags;                              // EntityGPUFlags 位或（Active=1 / Visible=2）
    uint8_t type[ENTITY_GPU_SLOT_COUNT];     // 槽 i != 0 ⇒ 实体有 ComponentType(i) 组件
    uint row[ENTITY_GPU_SLOT_COUNT];         // 槽 i 的 arena 行号；0 = 无（行 0 哨兵惯例）
    uint work_flags;                         // 工作期派生位；恒 0，等消费者定义（不造没人读的位）
    uint8_t _pad[40];                        // 显式填充：88 B 载荷 ⇒ 128 B 步长（对齐契约）
};

layout(buffer_reference, scalar, buffer_reference_align=64) readonly buffer EntityGPURef
{
    EntityGPUData rows[];
};

// 行数组本体：剔除循环的 index 是着色器手里的量 ⇒ 宏暴露 rows[]（camera 宏按下标
// 解引用的同族；下标不作答）。对齐按 128 B 行取 64。
#define entity_gpu_rows EntityGPURef(world_addresses.addr_entity_gpu).rows

#endif // HGL_ENTITY_GPU_GLSL
