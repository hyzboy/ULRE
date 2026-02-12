#pragma once

#include<hgl/graph/VKBufferAccessBase.h>

VK_NAMESPACE_BEGIN

/**
 * 结构化缓冲区访问器
 * 
 * CN: 将任意 C++ 结构体或类直接映射到 GPU 缓冲区，提供：
 * 1. 便捷的 CPU 端数据修改（就像访问普通 C++ 对象一样）
 * 2. 自动 dirty 追踪
 * 3. 统一的 Commit 接口（兼容所有缓冲区类型：GPUOnly, ReBAR, StagedBuffer, RingBuffer）
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
 * cam_accessor.Commit();  // 内部自动调用 Flush 如果需要
 * ```
 */
template<typename T>
class StructuredBufferAccessor:public BufferAccessBase
{
private:
    T *mapped_data;                 ///< 映射后的数据指针 / Mapped data pointer

    /**
     * CN: 内部 Map 操作
     * EN: Internal map operation
     */
    void MapInternal()
    {
        if(!buffer || mapped_data)
            return;

        void *ptr = buffer->Map(0, sizeof(T));
        if(ptr)
            mapped_data = static_cast<T*>(ptr);
    }

    /**
     * CN: 内部 Unmap 操作
     * EN: Internal unmap operation
     */
    void UnmapInternal()
    {
        if(!buffer || !mapped_data)
            return;

        buffer->Unmap();
        mapped_data = nullptr;
    }

public:

    /**
     * CN: 构造函数
     * EN: Constructor
     * 
     * \param buf DeviceBuffer 指针，必须至少能容纳 sizeof(T) 字节 / Buffer pointer, must hold sizeof(T) bytes
     * \param take_ownership 是否获取缓冲区的所有权，析构时删除 / Whether to take ownership (delete on dtor)
     */
    StructuredBufferAccessor(DeviceBuffer *buf, bool take_ownership = false)
        : BufferAccessBase()
        , mapped_data(nullptr)
    {
        SetBuffer(buf, take_ownership);
        if(buffer)
            MapInternal();
    }

    /**
     * CN: 构造函数（带 UBO 元数据）- 支持 DescriptorBinding::AddUBO()
     * EN: Constructor with UBO metadata - compatible with DescriptorBinding::AddUBO()
     * 
     * \param buf DeviceBuffer 指针
     * \param dst 描述符集合类型 / Descriptor set type
     * \param name UBO 名称 / UBO name
     * \param take_ownership 是否获取缓冲区的所有权
     */
    StructuredBufferAccessor(DeviceBuffer *buf, DescriptorSetType dst, const AnsiString &name, bool take_ownership = false)
        : BufferAccessBase()
        , mapped_data(nullptr)
    {
        SetBuffer(buf, take_ownership);
        SetUBOMeta(dst, name);
        if(buffer)
            MapInternal();
    }

    /**
     * CN: 构造函数（带 ShaderBufferDesc）- 支持来自 ShaderBufferSource 的初始化
     * EN: Constructor with ShaderBufferDesc - compatible with ShaderBufferSource
     * 
     * \param buf DeviceBuffer 指针
     * \param desc 着色器缓冲区描述符 / Shader buffer descriptor
     * \param take_ownership 是否获取缓冲区的所有权
     */
    StructuredBufferAccessor(DeviceBuffer *buf, const ShaderBufferDesc *desc, bool take_ownership = false)
        : BufferAccessBase()
        , mapped_data(nullptr)
    {
        SetBuffer(buf, take_ownership);
        SetUBOMeta(desc ? desc->set_type : DescriptorSetType::PerMaterial, desc ? desc->name : "");
        if(buffer)
            MapInternal();
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
    {
        MoveFrom(std::move(other));
        other.mapped_data = nullptr;
    }

    StructuredBufferAccessor& operator=(StructuredBufferAccessor&& other) noexcept
    {
        if(this != &other)
        {
            UnmapInternal();
            SetBuffer(nullptr, false);

            MoveFrom(std::move(other));
            mapped_data = other.mapped_data;
            other.mapped_data = nullptr;
        }
        return *this;
    }

    /**
     * CN: 绑定到新的缓冲区
     * EN: Bind to new buffer
     */
    void Bind(DeviceBuffer *buf, bool take_ownership = false)
    {
        UnmapInternal();
        SetBuffer(buf, take_ownership);

        if(buffer)
            MapInternal();
    }

    /**
     * CN: 检查是否有效
     * EN: Check if valid
     */
    bool IsValid() const { return buffer && mapped_data; }
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

    /**
     * CN: 提交修改到 GPU
     * 
     * 行为：
     * 1. 如果缓冲区有 StagedBuffer 关联，Unmap 会触发 MarkDirty
     * 2. 重新 Map，确保 StagedBuffer 的变更被标记
     * 3. 对于 ReBAR/CPUOnly 缓冲区，Unmap/Map 可能是无操作
     * 4. 对于需要显式 flush 的情况，调用 buffer->Flush()
     * 
     * EN: Commit changes to GPU
     * 
     * Behavior:
     * 1. If buffer has associated StagedBuffer, Unmap triggers MarkDirty
     * 2. Re-Map to ensure StagedBuffer changes are tracked
     * 3. For ReBAR/CPUOnly buffers, Unmap/Map may be no-ops
     * 4. For cases needing explicit flush, call buffer->Flush()
     * 
     * \return 是否执行了提交 / Whether committed
     */
    bool Commit()
    {
        if(!dirty || !buffer || !mapped_data)
            return false;

        // 显式 Flush 当前数据到 GPU
        // Explicitly flush current data to GPU
        buffer->Write(mapped_data, sizeof(T));

        // 对于 StagedBuffer，需要标记为 dirty 并检查提交
        // For StagedBuffer, mark as dirty to trigger staged buffer submission
        buffer->Flush(sizeof(T));

        dirty = false;
        return true;
    }

    /**
     * CN: 立即 Update 的便利方法（和旧 UBOInstance::Update() 兼容）
     * 对应旧的 DeviceBufferMap::Update() 行为
     * EN: Convenience method for immediate update (compatible with old UBOInstance::Update())
     * Maps to old DeviceBufferMap::Update() behavior
     */
    void ImmediateUpdate() const
    {
        if(!mapped_data || !buffer)
            return;

        buffer->Write(mapped_data, sizeof(T));
        buffer->Flush(sizeof(T));
        
        // Reset frame counter after successful commit
        const_cast<DeviceBuffer*>(buffer)->ResetFramesSinceUpdate();
    }

    /**
     * CN: 代理 DeviceBuffer::Write 方法 - 用于部分数据更新
     * EN: Proxy DeviceBuffer::Write method - for partial data update
     * 用于更新结构体中的某个字段而不是整个结构体
     */
    bool Write(const void *ptr, uint32_t offset, uint32_t size)
    {
        if(!buffer)
            return false;
        return buffer->Write(ptr, offset, size);
    }

    /**
     * CN: 代理 DeviceBuffer::Flush 方法
     * EN: Proxy DeviceBuffer::Flush method
     */
    void Flush(uint32_t size)
    {
        if(buffer)
            buffer->Flush(size);
    }

    void Flush()
    {
        if(buffer)
            buffer->Flush(sizeof(T));
    }

    // ===== UBOInstance 兼容接口 / UBOInstance Compatible Interface =====

    /**
     * CN: 获取描述符集合类型（UBOInstance 兼容）
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
     * CN: Update() 方法（UBOInstance 兼容）- 尝试自动 Flush（如果是 StagedBuffer）
     * EN: Update() method (UBOInstance compatible) - auto-flush if needed
     */
    void Update() const override
    {
        ImmediateUpdate();
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

VK_NAMESPACE_END
