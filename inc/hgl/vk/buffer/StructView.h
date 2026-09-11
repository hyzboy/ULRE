#pragma once

#include<hgl/vk/buffer/BufferView.h>

#include<type_traits>

namespace hgl::graph{

/**
 * 结构化缓冲区访问器
 *
 * CN: 将任意 C++ 结构体或类直接映射到 GPU 缓冲区，提供：
 * 1. 便捷的 CPU 端数据修改（像访问普通 C++ 对象一样）
 * 2. 自动 dirty 追踪
 * 3. 统一的 Commit 接口（兼容所有缓冲区类型：GPUOnly、ReBAR、StagedBuffer、RingBuffer）
 * 4. 自动 Map/Unmap 生命周期管理
 *
 * EN: Maps any C++ struct or class directly to GPU buffer, provides:
 * 1. Convenient CPU-side data modification (like accessing normal C++ objects)
 * 2. Automatic dirty tracking
 * 3. Unified Commit interface (compatible with all buffer types)
 * 4. Automatic Map/Unmap lifecycle management
 *
 * 使用示例 / Usage Example:
 * ```cpp
 * struct CameraData { glm::mat4 vp; glm::vec3 pos; };
 * auto camera_buf = device->CreateUBO(sizeof(CameraData));
 *
 * StructView<CameraData> cam_accessor(camera_buf);
 *
 * // 直接修改 CPU 端数据 / Modify CPU-side data directly
 * cam_accessor.Data()->vp = glm::mat4(1.0f);
 * cam_accessor.Data()->pos = glm::vec3(0, 0, 10);
 *
 * // 自动标记 dirty 并提交到 GPU
 * cam_accessor.Commit();  // 内部自动调用 Flush 如有需要
 * ```
 */
template<typename T>
class StructView:public BufferView
{
private:
    friend class VulkanDevice;

public:
    VkDeviceSize aligned_size = 0;      ///< 映射窗口字节数（= buffer 大小）
    bool initialized = false;
    // 映射基址与脏标记统一由 BufferView 的窗口机制持有：
    // 不再自持 mapped_data / dirty（消除与 L2 的双记账）

    /**
     * CN: 内部 Map 操作（窗口 = 整块 buffer）
     * EN: Internal map operation
     */
    void MapInternal()
    {
        MapWindow(0, aligned_size);
    }

    void InitDefaultsIfNeeded()
    {
        if(initialized || !HasWindow())
            return;

        if constexpr (std::is_array_v<T>)
        {
            using Element = std::remove_extent_t<T>;
            constexpr size_t kCount = std::extent_v<T>;

            for(size_t i = 0; i < kCount; ++i)
                (*Data())[i] = Element();

            ImmediateUpdate();
        }
        else if constexpr (std::is_default_constructible_v<T>)
        {
            *Data() = T();
            ImmediateUpdate();
        }

        initialized = true;
    }

    StructView(BufferOwner *buf, VkDeviceSize aligned_size_param, bool take_ownership)
        : BufferView()
        , aligned_size(aligned_size_param)
    {
        SetBuffer(buf);
        MapInternal();
        InitDefaultsIfNeeded();
    }

    StructView(BufferOwner *buf, bool take_ownership = false)
        : BufferView()
        , aligned_size(buf ? buf->GetSize() : 0)
    {
        SetBuffer(buf);
        MapInternal();
        InitDefaultsIfNeeded();
    }

public:
    static StructView *Create(BufferOwner *buf, bool take_ownership = false)
    {
        return buf ? new StructView(buf, take_ownership) : nullptr;
    }

    /**
     * CN: 析构函数 - 自动 Unmap 和可选的 buffer 删除
     * EN: Destructor - auto unmap and optional buffer cleanup
     */
    ~StructView()
    {
        // 窗口由 BufferView 析构时统一解锁（UnmapInternal 已删除）
    }

    // 禁止拷贝 / Disable copy
    StructView(const StructView&) = delete;
    StructView& operator=(const StructView&) = delete;

    // 允许移动 / Allow move
    StructView(StructView&& other) noexcept
        : BufferView()
        , aligned_size(other.aligned_size)
    {
        MoveFrom(std::move(other));     // 窗口状态随 MoveFrom 一起搬
        other.aligned_size = 0;
    }

    StructView& operator=(StructView&& other) noexcept
    {
        if(this != &other)
        {
            UnmapWindow();
            SetBuffer(nullptr);

            MoveFrom(std::move(other));
            aligned_size = other.aligned_size;
            other.aligned_size = 0;
        }
        return *this;
    }

public:

public:

    /**
     * CN: 检查是否有效
     * EN: Check if valid
     */
    bool IsValid() const { return gpu_buf && HasWindow(); }
    operator bool() const { return IsValid(); }

    /**
     * CN: 获取结构体数据指针
     * 修改此指针指向的数据后，需调用 MarkDirty() 或 Commit()
     * EN: Get struct data pointer
     * After modifying data through this pointer, call MarkDirty() or Commit()
     */
    T* Data() { return static_cast<T*>(GetWindowData()); }
    const T* Data() const { return static_cast<const T*>(GetWindowData()); }

    /**
     * CN: 箭头操作符 - 直接访问结构体成员
     * EN: Arrow operator - direct struct member access
     */
    T* operator->() { return Data(); }
    const T* operator->() const { return static_cast<const T*>(GetWindowData()); }

    /**
     * CN: 解引用操作符
     * EN: Dereference operator
     */
    T& operator*() { return *Data(); }
    const T& operator*() const { return *static_cast<const T*>(GetWindowData()); }

    /**
     * CN: 标记为 dirty
     * EN: Mark as dirty
     */
    void MarkDirty()
    {
        // 脏范围 = 本视图窗口（整块 buffer）交 L2；视图不自持 dirty
        MarkWindowDirty();
    }

    /**
     * CN: 检查是否 dirty
     * EN: Check if dirty
     */
    bool IsDirty() const { return gpu_buf ? gpu_buf->IsDirty() : false; }

public:

    /**
     * CN: 便利方法：修改数据并标记 dirty
     * EN: Convenience method: assign data and mark dirty
     */
    void Update(const T& data)
    {
        if(!HasWindow())
            return;

        *Data() = data;
        MarkWindowDirty();      // 拷贝数据 + 置脏
    }

public:

    /**
     * CN: 提交：把窗口范围标脏交 L2（数据本就在映射窗口里，无自我拷贝）
     * EN: Commit: mark the window range dirty on L2
     */
    void Commit()
    {
        if(!gpu_buf || !HasWindow())
            return;

        MarkWindowDirty();
    }

private:

    /**
     * CN: 立即 Update 的便利方法（和旧 UBOInstance::Update() 兼容）
     * 对应旧的 DeviceBufferMap::Update() 行为
     * EN: Convenience method for immediate update (compatible with old UBOInstance::Update())
     * Maps to old DeviceBufferMap::Update() behavior
     */
private:

    void ImmediateUpdate() const
    {
        if(!HasWindow() || !gpu_buf)
            return;

        gpu_buf->MarkDirty(0, static_cast<VkDeviceSize>(sizeof(T)));
    }

    /**
     * CN: 代理 DeviceBuffer::Write 方法 - 用于部分数据更新
     * EN: Proxy DeviceBuffer::Write method - for partial data update
     * 用于更新结构体中的某个字段而不是整个结构体
     */
public:

    bool Write(const void *ptr, uint32_t offset, uint32_t size)
    {
        // BufferView::Write guards on gpu_buf internally.
        return BufferView::Write(ptr, offset, size);
    }

    /**
     * CN: 获取结构体大小（编译时常量）
     * EN: Get struct size (compile-time constant)
     */
    static constexpr VkDeviceSize GetSize()
    {
        return sizeof(T);
    }
};

}//namespace hgl::graph
