#pragma once

#include <hgl/type/ValueArray.h>
#include <hgl/log/Log.h>
#include <cstdint>

namespace hgl
{
    namespace graph
    {
        class VulkanDevice;
        class DeviceBuffer;
    }
}

namespace hgl::ecs
{
    /**
     * @brief 二级绘制索引表存储器 (DrawItemIDBuffer)
     * 
     * 管理每帧暂态离散 RenderItemHandle 的线性排布与 GPU 缓冲映射。
     * 当一组渲染项无法进行连号区间折叠 (Run-Length Compaction) 时，
     * 将其离散 Handle 顺序追加至本存储器，供着色器间接查表。
     */
    class DrawItemIDStorage
    {
        OBJECT_LOGGER

    private:
        hgl::ValueArray<uint32_t> ids;                ///< CPU 端存储的 Handle ID 数组
        graph::DeviceBuffer *device_buffer = nullptr; ///< GPU 设备端 SSBO
        uint32_t gpu_capacity = 0;                    ///< 当前 GPU 缓冲容量（元素个数）
        uint64_t gpu_address = 0;                     ///< GPU 物理设备地址 (BDA)
        uint64_t external_gpu_address = 0;            ///< 外部 GPU 设备地址覆盖 (用于 100% GPU-Driven 模式)
        bool is_dirty = false;                        ///< 当前帧是否有新数据需要同步

        uint32_t last_frame_uploaded_bytes = 0;       ///< 上一帧上传的字节数
        uint32_t last_frame_item_count = 0;           ///< 上一帧容纳的项目数

    public:
        DrawItemIDStorage();
        ~DrawItemIDStorage();

        /// 帧重置：重置写入计数，保留已分配的 GPU 缓冲容量
        void Reset();

        /// 设置外部 GPU 设备地址覆盖（用于 100% GPU-Driven 模式）
        void SetExternalGPUAddress(const uint64_t addr) { external_gpu_address = addr; }

        /// 设置外部 GPU 缓冲覆盖
        void SetExternalGPUBuffer(graph::DeviceBuffer *buf, graph::VulkanDevice *dev = nullptr);

        /// 清除外部 GPU 地址覆盖
        void ClearExternalGPUAddress() { external_gpu_address = 0; }

        /// 是否存在外部 GPU 设备地址覆盖
        bool HasExternalGPUAddress() const { return external_gpu_address != 0; }

        /// 批量追加离散 Handle，返回在二级缓冲中的起始偏移量 (offset)
        uint32_t Append(const uint32_t *handles, const uint32_t count);

        /// 追加单个离散 Handle，返回其偏移量
        uint32_t Append(const uint32_t handle);

        /// 获取当前帧累积的 Handle 数量
        uint32_t GetCount() const { return static_cast<uint32_t>(ids.GetCount()); }

        /// 获取 CPU 端数据指针
        const uint32_t *GetData() const { return ids.GetData(); }

        /// 获取 GPU 显存物理地址 (BDA, 16 字节对齐)
        uint64_t GetGPUAddress() const { return external_gpu_address != 0 ? external_gpu_address : gpu_address; }

        /// 获取 GPU 设备缓冲区指针
        graph::DeviceBuffer *GetDeviceBuffer() const { return device_buffer; }

        /// 确保存储容量至少达到 min_capacity
        bool EnsureGPUBuffer(graph::VulkanDevice *dev, const uint32_t min_capacity);

        /// 同步当前帧数据至 GPU
        bool SyncToGPU(graph::VulkanDevice *dev);

        /// 释放 GPU 显存缓冲
        void ReleaseGPUBuffer();

        /// 统计信息查询
        uint32_t GetLastFrameUploadedBytes() const { return last_frame_uploaded_bytes; }
        uint32_t GetLastFrameItemCount() const { return last_frame_item_count; }
    };
}
