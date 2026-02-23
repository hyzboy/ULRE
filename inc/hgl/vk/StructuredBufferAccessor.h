#pragma once

#include<hgl/vk/VKBufferAccessBase.h>

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
 * StructuredBufferAccessor<CameraData> cam_accessor(camera_buf);
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
class StructuredBufferAccessor:public BufferAccessBase
{
public:
    T *mapped_data;                 ///< 映射后的数据指针 / Mapped data pointer
    VkDeviceSize aligned_size = 0;
    bool initialized = false;
    friend class VulkanDevice;

private:
    // 跟踪结构体数据是否被 Update() 修改过，用于 CommitInternal() 决策
    // 与 GPU 上传无关，GPU dirty 由底层 Write()/Unmap() 路径自动维护
    bool dirty = false;

    /**
     * CN: 内部 Map 操作
     * EN: Internal map operation
     */
    void MapInternal()
    {
        // gpu_buf cached in BufferAccessBase::SetBuffer(); no DeviceBuffer chain.
        if(!gpu_buf || mapped_data)
            return;

        void *ptr = gpu_buf->Map(0, aligned_size);
        if(ptr)
            mapped_data = static_cast<T*>(ptr);
    }

    void InitDefaultsIfNeeded()
    {
        if(initialized || !mapped_data)
            return;

        if constexpr (std::is_array_v<T>)
        {
            using Element = std::remove_extent_t<T>;
            constexpr size_t kCount = std::extent_v<T>;

            for(size_t i = 0; i < kCount; ++i)
                (*mapped_data)[i] = Element();

            ImmediateUpdate();
        }
        else if constexpr (std::is_default_constructible_v<T>)
        {
            *mapped_data = T();
            ImmediateUpdate();
        }

        initialized = true;
    }

    StructuredBufferAccessor(DeviceBuffer *buf, VkDeviceSize aligned_size_param, bool take_ownership)
        : BufferAccessBase()
        , mapped_data(nullptr)
        , aligned_size(aligned_size_param)
    {
        SetBuffer(buf);
        if(gpu_buf)
            MapInternal();
        InitDefaultsIfNeeded();
    }

    /**
     * CN: 内部 Unmap 操作
     * EN: Internal unmap operation
     */
    void UnmapInternal()
    {
        if(!gpu_buf || !mapped_data)
            return;

        gpu_buf->Unmap();
        mapped_data = nullptr;
    }

    StructuredBufferAccessor(DeviceBuffer *buf, bool take_ownership = false)
        : BufferAccessBase()
        , mapped_data(nullptr)
        , aligned_size(buf ? buf->GetSize() : 0)
    {
        SetBuffer(buf);
        if(gpu_buf)
            MapInternal();
        InitDefaultsIfNeeded();
    }

    StructuredBufferAccessor(DeviceBuffer *buf, DescriptorSetType dst, const AnsiString &name, bool take_ownership = false)
        : BufferAccessBase()
        , mapped_data(nullptr)
        , aligned_size(buf ? buf->GetSize() : 0)
    {
        SetBuffer(buf);
        SetUBOMeta(dst, name);
        if(gpu_buf)
            MapInternal();
        InitDefaultsIfNeeded();
    }

    StructuredBufferAccessor(DeviceBuffer *buf, const ShaderBufferDesc *desc, bool take_ownership = false)
        : BufferAccessBase()
        , mapped_data(nullptr)
        , aligned_size(buf ? buf->GetSize() : 0)
    {
        SetBuffer(buf);
        SetUBOMeta(desc ? desc->set_type : DescriptorSetType::PerMaterial, desc ? desc->name : "");
        if(gpu_buf)
            MapInternal();
        InitDefaultsIfNeeded();
    }

public:
    static StructuredBufferAccessor *Create(DeviceBuffer *buf, bool take_ownership = false)
    {
        return buf ? new StructuredBufferAccessor(buf, take_ownership) : nullptr;
    }

    static StructuredBufferAccessor *Create(DeviceBuffer *buf, DescriptorSetType dst, const AnsiString &name, bool take_ownership = false)
    {
        return buf ? new StructuredBufferAccessor(buf, dst, name, take_ownership) : nullptr;
    }

    static StructuredBufferAccessor *Create(DeviceBuffer *buf, const ShaderBufferDesc *desc, bool take_ownership = false)
    {
        return buf ? new StructuredBufferAccessor(buf, desc, take_ownership) : nullptr;
    }

    /**
     * CN: 析构函数 - 自动 Unmap 和可选的 buffer 删除
     * EN: Destructor - auto unmap and optional buffer cleanup
     */
    ~StructuredBufferAccessor()
    {
        UnmapInternal();
    }

    // 禁止拷贝 / Disable copy
    StructuredBufferAccessor(const StructuredBufferAccessor&) = delete;
    StructuredBufferAccessor& operator=(const StructuredBufferAccessor&) = delete;

    // 允许移动 / Allow move
    StructuredBufferAccessor(StructuredBufferAccessor&& other) noexcept
        : BufferAccessBase()
        , mapped_data(other.mapped_data)
        , aligned_size(other.aligned_size)
    {
        MoveFrom(std::move(other));
        other.mapped_data = nullptr;
        other.aligned_size = 0;
    }

    StructuredBufferAccessor& operator=(StructuredBufferAccessor&& other) noexcept
    {
        if(this != &other)
        {
            UnmapInternal();
            SetBuffer(nullptr, false);

            MoveFrom(std::move(other));
            mapped_data = other.mapped_data;
            aligned_size = other.aligned_size;
            other.mapped_data = nullptr;
            other.aligned_size = 0;
        }
        return *this;
    }

public:

    /**
     * CN: 绑定到新的缓冲区
     * EN: Bind to new buffer
     */
    void Bind(DeviceBuffer *buf, bool take_ownership = false)
    {
        UnmapInternal();
        aligned_size = buf ? buf->GetSize() : 0;
        SetBuffer(buf);

        if(gpu_buf)
            MapInternal();
    }

public:

    /**
     * CN: 检查是否有效
     * EN: Check if valid
     */
    bool IsValid() const { return gpu_buf && mapped_data; }
    operator bool() const { return IsValid(); }

    /**
     * CN: 获取底层缓冲区
     * EN: Get underlying buffer
     */
    DeviceBuffer* GetBuffer() { return buffer; }
    const DeviceBuffer* GetBuffer() const { return buffer; }

    /**
     * CN: 获取结构体数据指针
     * 修改此指针指向的数据后，需调用 MarkDirty() 或 Commit()
     * EN: Get struct data pointer
     * After modifying data through this pointer, call MarkDirty() or Commit()
     */
    T* Data() { return mapped_data; }
    const T* Data() const { return mapped_data; }

    /**
     * CN: 箭头操作符 - 直接访问结构体成员
     * EN: Arrow operator - direct struct member access
     */
    T* operator->() { return mapped_data; }
    const T* operator->() const { return mapped_data; }

    /**
     * CN: 解引用操作符
     * EN: Dereference operator
     */
    T& operator*() { return *mapped_data; }
    const T& operator*() const { return *mapped_data; }

    /**
     * CN: 标记为 dirty
     * EN: Mark as dirty
     */
    void MarkDirty() { dirty = true; }

    /**
     * CN: 检查是否 dirty
     * EN: Check if dirty
     */
    bool IsDirty() const { return dirty; }

public:

    /**
     * CN: 便利方法：修改数据并标记 dirty
     * EN: Convenience method: assign data and mark dirty
     */
    void Update(const T& data)
    {
        if(!mapped_data)
            return;
        *mapped_data = data;
        dirty = true;
    }

private:

    /**
     * Internal commit path used by BufferCommitQueue-driven Update only.
     *
     * \return whether committed
     */
    bool CommitInternal()
    {
        if(!dirty || !gpu_buf || !mapped_data)
            return false;

        gpu_buf->Write(mapped_data, 0, sizeof(T));
        dirty = false;
        return true;
    }

public:

    /**
     * CN: 立即 Update 的便利方法（和旧 UBOInstance::Update() 兼容）
     * 对应旧的 DeviceBufferMap::Update() 行为
     * EN: Convenience method for immediate update (compatible with old UBOInstance::Update())
     * Maps to old DeviceBufferMap::Update() behavior
     */
private:

    void ImmediateUpdate() const
    {
        if(!mapped_data || !gpu_buf)
            return;

        gpu_buf->Write(mapped_data, 0, sizeof(T));
    }

    /**
     * CN: 代理 DeviceBuffer::Write 方法 - 用于部分数据更新
     * EN: Proxy DeviceBuffer::Write method - for partial data update
     * 用于更新结构体中的某个字段而不是整个结构体
     */
public:

    bool Write(const void *ptr, uint32_t offset, uint32_t size)
    {
        // BufferAccessBase::Write guards on gpu_buf internally.
        return BufferAccessBase::Write(ptr, offset, size);
    }

    // ===== UBOInstance 兼容接口 / UBOInstance Compatible Interface =====

    /**
     * CN: 获取描述符集类型（UBOInstance 兼容）
     * EN: Get descriptor set type (UBOInstance compatible)
     */
    const DescriptorSetType& set_type() const { return desc_set_type; }

    /**
     * CN: 获取 UBO 名称（UBOInstance 兼容）
     * EN: Get UBO name (UBOInstance compatible)
     */
    const AnsiString& name() const { return ubo_name; }

    /**
     * CN: 获取底层 DeviceBuffer（UBOInstance 兼容）
     * EN: Get underlying DeviceBuffer (UBOInstance compatible)
     */
    DeviceBuffer* ubo() const { return buffer; }

    /**
     * CN: Update() 方法（UBOInstance 兼容） 尝试自动 Flush（如果是 StagedBuffer）
     * EN: Update() method (UBOInstance compatible) - auto-flush if needed
     */
    void Update() const override
    {
        const_cast<StructuredBufferAccessor<T>*>(this)->CommitInternal();
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
