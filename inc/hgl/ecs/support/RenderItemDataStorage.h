#pragma once

#include <hgl/graph/render/RenderItemDescriptor.h>
#include <hgl/type/ValueArray.h>
#include <hgl/log/Log.h>
#include <cstdint>

namespace hgl::graph
{
    class DeviceBuffer;
    class VulkanDevice;
}

namespace hgl::ecs
{
    using graph::RenderItemDescriptor;
    using graph::RenderItemHandle;
    using graph::INVALID_RENDER_ITEM_HANDLE;

    /**
     * RenderItemDataStorage - 全局图元描述符池 (16B 4-ID)
     *
     * 职责：
     * 1. CPU 镜像存储与槽位分配 (支持单槽复用与连续连号块分配)
     * 2. 局部属性原地修改 (transform_id, geometry_id, material_id, texture_id)
     * 3. 增量脏范围跟踪 (Dirty Range Tracking)
     * 4. GPU 显存镜像同步 (自动扩容与增量写入 GlobalRenderItemBuffer SSBO)
     */
    class RenderItemDataStorage
    {
        OBJECT_LOGGER

    private:
        // CPU 连续描述符数组 (16B 逐项紧密排布)
        hgl::ValueArray<RenderItemDescriptor> items;

        // 空闲单槽复用表 (LIFO)
        hgl::ValueArray<RenderItemHandle> free_list;

        // 脏范围标记
        uint32_t dirty_min = UINT32_MAX;
        uint32_t dirty_max = 0;
        bool is_dirty = false;

        // GPU 镜像缓冲
        graph::DeviceBuffer *device_buffer = nullptr;
        uint32_t gpu_capacity = 0;
        uint64_t gpu_address = 0;

    public:
        RenderItemDataStorage();
        ~RenderItemDataStorage();

        // 禁用拷贝，允许移动
        RenderItemDataStorage(const RenderItemDataStorage &) = delete;
        RenderItemDataStorage &operator=(const RenderItemDataStorage &) = delete;

    public:
        // ── 槽位分配与释放 ──

        /// 分配单个描述符槽位 (优先复用 free_list)
        RenderItemHandle Allocate();

        /// 分配单个描述符槽位并赋予初始值
        RenderItemHandle Allocate(const RenderItemDescriptor &desc);

        /// 批量分配连续连号槽位区间 [base, base + count)
        /// 为静态网格簇与多实例直通渲染保证首实例索引连号
        RenderItemHandle AllocateContiguous(uint32_t count);

        /// 释放指定槽位
        bool Release(RenderItemHandle handle);

        /// 释放连续槽位区间
        bool ReleaseContiguous(RenderItemHandle base, uint32_t count);

        /// 清空所有分配与状态
        void Clear();

    public:
        // ── 槽位访问与原地修改 ──

        const RenderItemDescriptor *Get(RenderItemHandle handle) const;
        RenderItemDescriptor *Get(RenderItemHandle handle);

        bool Set(RenderItemHandle handle, const RenderItemDescriptor &desc);
        bool SetTransformID(RenderItemHandle handle, uint32_t transform_id);
        bool SetGeometryID(RenderItemHandle handle, uint32_t geometry_id);
        bool SetMaterialID(RenderItemHandle handle, uint32_t material_id);
        bool SetTextureID(RenderItemHandle handle, uint32_t texture_id);
        bool Set4ID(RenderItemHandle handle, uint32_t transform_id, uint32_t geometry_id, uint32_t material_id, uint32_t texture_id);

        bool IsValidHandle(RenderItemHandle handle) const;

    public:
        // ── 容量与计数查询 ──

        uint32_t GetCount() const { return static_cast<uint32_t>(items.GetCount()); }
        uint32_t GetCapacity() const { return static_cast<uint32_t>(items.GetAllocCount()); }
        uint32_t GetFreeCount() const { return static_cast<uint32_t>(free_list.GetCount()); }
        uint32_t GetActiveCount() const { return GetCount() >= GetFreeCount() ? (GetCount() - GetFreeCount()) : 0; }
        const RenderItemDescriptor *GetData() const { return items.GetData(); }

    public:
        // ── 脏范围跟踪 ──

        void MarkDirty(uint32_t index);
        void MarkRangeDirty(uint32_t start, uint32_t count);
        void ClearDirty();
        bool IsDirty() const { return is_dirty; }
        bool GetDirtyRange(uint32_t &out_min, uint32_t &out_max) const;

    public:
        // ── GPU 显存镜像同步 ──

        /// 预分配或确保 GPU SSBO 满足最小容量
        bool EnsureGPUBuffer(graph::VulkanDevice *dev, uint32_t min_capacity = 0);

        /// 增量同步发生变更的描述符至 GPU SSBO
        bool SyncToGPU(graph::VulkanDevice *dev);

        /// 释放 GPU 缓冲资源
        void ReleaseGPUBuffer();

        /// 获取 GPU SSBO 句柄与 BDA
        graph::DeviceBuffer *GetDeviceBuffer() const { return device_buffer; }
        uint64_t GetGPUAddress() const { return gpu_address; }
        uint32_t GetGPUCapacity() const { return gpu_capacity; }
    };
}//namespace hgl::ecs
