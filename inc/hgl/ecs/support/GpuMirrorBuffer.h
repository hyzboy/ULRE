#pragma once

// ═══════════════════════════════════════════════════════════════════════════
// 世界表「CPU 数组 → GPU SSBO 镜像」的**共用扩容策略**（审计 §2.2）
//
// 背景：`RenderItemDataStorage` 与 `DrawItemIDStorage` 是两套同构的 GPU 镜像索引存储
// （字段相同：device_buffer / gpu_capacity / gpu_address），各自抄了一份 `EnsureGPUBuffer`：
// 早退判据、默认容量 1024、已有容量则 ×2、下限 64、建 SSBO、删旧缓冲、记容量 + BDA、两条日志
// —— 逐字相同，只差 element_size、日志里的表名、以及"是否迁移已有行"。
// 两处各抄一份的代价 = **两套会分叉的增长策略**（改一处忘另一处），正是本文件要消掉的东西。
//
// 分工（有意为之）：
//   · 本件只管**策略与缓冲骨架**，**不打印任何日志** —— 各表的日志用对象日志宏
//     （`OBJECT_LOGGER` 的 `LogInfo`/`LogError` 带 `typeid(*this)` 上下文，挪进自由函数会 C2355），
//     所以日志留在调用方，文本逐字不变；
//   · 各表保留自己的：CPU 数组、脏跟踪、日志文案、迁移钩子。
// ═══════════════════════════════════════════════════════════════════════════

#include <hgl/vk/VKDevice.h>
#include <hgl/vk/buffer/DeviceBuffer.h>
#include <algorithm>
#include <cstdint>

namespace hgl::ecs
{
    /// 镜像缓冲容量下限（两处共用，勿各自硬编码）
    inline constexpr uint32_t kGpuMirrorMinCapacity = 64;

    /// 无显式需求时的默认容量（两处共用）
    inline constexpr uint32_t kGpuMirrorDefaultCapacity = 1024;

    /// 下一次需要多大：min_capacity 优先；已有容量则至少 ×2；下限 `kGpuMirrorMinCapacity`。
    inline uint32_t NextGpuMirrorCapacity(const uint32_t gpu_capacity, const uint32_t min_capacity)
    {
        uint32_t target = min_capacity > 0 ? min_capacity : kGpuMirrorDefaultCapacity;

        if (gpu_capacity > 0)
            target = std::max(target, gpu_capacity * 2);

        if (target < kGpuMirrorMinCapacity)
            target = kGpuMirrorMinCapacity;

        return target;
    }

    /// 现有缓冲是否已够用（够用则调用方按"什么都不做"处理：**不重标脏、不迁移**）
    inline bool GpuMirrorCapacitySufficient(const graph::DeviceBuffer *buffer,
                                            const uint32_t gpu_capacity,
                                            const uint32_t min_capacity)
    {
        return buffer && gpu_capacity >= min_capacity && min_capacity > 0;
    }

    /// 一次扩容尝试的结果（调用方据此决定日志与脏标记）
    enum class GpuMirrorResult
    {
        NoDevice,       ///< 设备为空 ⇒ 失败且**不报错**（与旧实现逐字一致）
        Unchanged,      ///< 容量已够 ⇒ 未动缓冲（调用方应"什么都不做"：不重标脏、不迁移）
        Reallocated,    ///< 已建新缓冲并换上
        Failed,         ///< `CreateSSBO` 失败 ⇒ 调用方按自己的表名报错
    };

    /// 确保镜像缓冲存在且容量 ≥ min_capacity（容量够则 `Unchanged`，不碰缓冲）。
    ///   migrate            ：新缓冲建好、旧缓冲释放**之前**调用（签名 (new_buffer, target_capacity)）
    ///   out_target_capacity：需要时回填目标容量（`Failed` 时供调用方拼报错文案）
    template<typename MigrateFn>
    GpuMirrorResult EnsureGpuMirrorBuffer(graph::VulkanDevice *dev,
                                          graph::DeviceBuffer *&device_buffer,
                                          uint32_t &gpu_capacity,
                                          uint64_t &gpu_address,
                                          const uint32_t min_capacity,
                                          const uint32_t element_size,
                                          uint32_t &out_target_capacity,
                                          MigrateFn &&migrate)
    {
        if (!dev)
            return GpuMirrorResult::NoDevice;

        if (GpuMirrorCapacitySufficient(device_buffer, gpu_capacity, min_capacity))
            return GpuMirrorResult::Unchanged;

        const uint32_t target_capacity = NextGpuMirrorCapacity(gpu_capacity, min_capacity);
        const VkDeviceSize byte_size = static_cast<VkDeviceSize>(element_size) * target_capacity;

        out_target_capacity = target_capacity;

        graph::DeviceBuffer *new_buffer = dev->CreateSSBO(byte_size,
                                                         nullptr,
                                                         graph::BufferAllocPolicy::Auto,
                                                         graph::SharingMode::Exclusive);
        if (!new_buffer)
            return GpuMirrorResult::Failed;

        migrate(new_buffer, target_capacity);

        if (device_buffer)
        {
            delete device_buffer;
            device_buffer = nullptr;
        }

        device_buffer = new_buffer;
        gpu_capacity = target_capacity;
        gpu_address = dev->GetBufferDeviceAddressAligned16(device_buffer->GetBuffer());

        return GpuMirrorResult::Reallocated;
    }
}
