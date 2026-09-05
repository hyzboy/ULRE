#pragma once

#include<hgl/vk/VKBufferAccessBase.h>
#include<hgl/mtl/MaterialRecipe.h>     ///< for mtl::SSBOType / mtl::SSBOBinding

namespace hgl::graph{

/**
 * 结构化缓冲区数组访问器
 *
 * CN: 将任意 C++ 结构体数组直接映射到 GPU SSBO 缓冲区，提供：
 * 1. 类型安全的 operator[] 元素访问（像访问普通 C++ 数组一样）
 * 2. 自动 dirty 追踪
 * 3. 统一的 Commit 接口（兼容所有缓冲区类型）
 * 4. 自动 Map/Unmap 生命周期管理
 * 5. 内置 SSBO ID 存储（由 ResourceDomainManager 分配）
 *
 * EN: Maps any C++ struct array directly to a GPU SSBO buffer, providing:
 * 1. Type-safe operator[] element access (like a normal C++ array)
 * 2. Automatic dirty tracking
 * 3. Unified Commit interface (compatible with all buffer types)
 * 4. Automatic Map/Unmap lifecycle management
 * 5. Built-in SSBO ID storage (assigned by ResourceDomainManager)
 *
 * 典型用途 / Typical usage:
 *   材质实例颜色数组 SSBO、PBRSurface 实例数据 SSBO 等
 *
 * 使用示例 / Usage Example:
 * ```cpp
 * // 一步式创建，ID 自动分配并存储在 accessor 内
 * auto* acc = domain_manager->AllocateArrayAccessor<Color4f>(
 *     SSBOType::PBRSurface, "MySSBO", DRAW_COUNT);
 *
 * // 直接用 accessor 里的 type+id 注册 recipe 绑定
 * UpsertRecipeSSBOAssetBinding(recipe, name, acc->GetSSBOBinding());
 *
 * // 写入元素
 * for (uint32_t i = 0; i < acc->GetCount(); i++)
 *     (*acc)[i] = GetColor4f(colors[i], 1.0f);
 *
 * // 提交到 GPU
 * acc->Commit();
 * ```
 */
template<typename T>
class SSBOArrayAccessor : public BufferAccessBase
{
private:
    T*            mapped_data   = nullptr;                     ///< 持久映射的数组基址 / Persistently mapped array base
    uint32_t      element_count = 0;                           ///< 数组元素数量 / Element count
    uint32_t      ssbo_id       = 0;                           ///< 分配到的 SSBO ID（由 ResourceDomainManager 写入）
    mtl::SSBOType ssbo_type     = mtl::SSBOType::UserDefined;  ///< SSBO 类型（由 ResourceDomainManager 写入）
    bool          dirty         = false;                       ///< CPU 侧是否有未提交的修改
    uint32_t      stride_bytes  = 0;                           ///< 行距字节数（0=sizeof(T) 紧密排布；Arena 路径=sizeof(T) 且 16B 对齐）
    bool          host_direct   = false;                       ///< true=HOST_COHERENT 直写（Commit 为 no-op）
    bool          owned_buffer  = false;                       ///< true=析构时释放 buffer（独立行缓冲持有）

    friend class VulkanDevice;
    friend class ResourceDomainManager;

private:

    void MapInternal()
    {
        if (!gpu_buf || mapped_data)
            return;

        void *ptr = gpu_buf->Map(0, static_cast<VkDeviceSize>(sizeof(T)) * element_count);
        if (ptr)
            mapped_data = static_cast<T*>(ptr);
    }

    void UnmapInternal()
    {
        if (!gpu_buf || !mapped_data)
            return;

        gpu_buf->Unmap();
        mapped_data = nullptr;
    }

    explicit SSBOArrayAccessor(VkBufferOwner *buf, uint32_t count)
        : BufferAccessBase()
        , element_count(count)
    {
        SetBuffer(buf);
        if (gpu_buf)
            MapInternal();
    }

    // Arena+BDA 路径：直写宿主窗口（mapped_data 即行段基址）
    explicit SSBOArrayAccessor(void *host_base, uint32_t count, uint32_t in_stride)
        : BufferAccessBase()
        , element_count(count)
        , stride_bytes(in_stride)
        , host_direct(true)
    {
        mapped_data = static_cast<T *>(host_base);
    }

    bool CommitInternal()
    {
        if (host_direct)
        {
            // HOST_COHERENT 直写：operator[] 赋值即已生效
            dirty = false;
            return true;
        }

        if (!dirty || !gpu_buf || !mapped_data)
            return false;

        gpu_buf->Write(mapped_data, 0, static_cast<uint32_t>(sizeof(T)) * element_count);
        dirty = false;
        return true;
    }

public:

    /**
     * CN: 工厂方法 —— 从已有 VkBufferOwner 创建数组访问器
     * EN: Factory — create from an existing VkBufferOwner
     *
     * @param buf   已创建的 GPU 缓冲区（大小须 >= sizeof(T) * count）
     * @param count 数组元素数量
     */
    static SSBOArrayAccessor* Create(VkBufferOwner *buf, uint32_t count)
    {
        if (!buf || count == 0)
            return nullptr;

        return new SSBOArrayAccessor(buf, count);
    }

    ~SSBOArrayAccessor()
    {
        UnmapInternal();

        if (owned_buffer && buffer)
            delete buffer;      // VkBufferOwner 析构链释放 IGPUBuffer/DeviceMemory/VkBuffer

        buffer      = nullptr;
        gpu_buf     = nullptr;
        mapped_data = nullptr;
    }

    // 禁止拷贝 / Disable copy
    SSBOArrayAccessor(const SSBOArrayAccessor&) = delete;
    SSBOArrayAccessor& operator=(const SSBOArrayAccessor&) = delete;

    // 允许移动 / Allow move
    SSBOArrayAccessor(SSBOArrayAccessor&& other) noexcept
        : BufferAccessBase()
        , mapped_data(other.mapped_data)
        , element_count(other.element_count)
        , ssbo_id(other.ssbo_id)
        , ssbo_type(other.ssbo_type)
        , dirty(other.dirty)
    {
        MoveFrom(std::move(other));
        other.mapped_data   = nullptr;
        other.element_count = 0;
        other.ssbo_id       = 0;
        other.ssbo_type     = mtl::SSBOType::UserDefined;
        other.dirty         = false;
    }

    SSBOArrayAccessor& operator=(SSBOArrayAccessor&& other) noexcept
    {
        if (this != &other)
        {
            UnmapInternal();
            SetBuffer(nullptr, false);

            MoveFrom(std::move(other));
            mapped_data   = other.mapped_data;
            element_count = other.element_count;
            ssbo_id       = other.ssbo_id;
            ssbo_type     = other.ssbo_type;
            dirty         = other.dirty;

            other.mapped_data   = nullptr;
            other.element_count = 0;
            other.ssbo_id       = 0;
            other.ssbo_type     = mtl::SSBOType::UserDefined;
            other.dirty         = false;
        }
        return *this;
    }

public:

    /**
     * CN: 检查是否有效（已映射且元素数 > 0）
     * EN: Check if valid (mapped and non-empty)
     */
    bool IsValid() const { return gpu_buf && mapped_data && element_count > 0; }
    operator bool() const { return IsValid(); }

    /**
     * CN: 返回此访问器对应的 SSBO ID（由 ResourceDomainManager 分配时写入）
     * EN: Return the SSBO ID assigned by ResourceDomainManager.
     */
    uint32_t GetSSBOId() const { return ssbo_id; }

    /**
     * CN: 取得行缓冲所有权：析构时释放整个 DeviceBuffer。
     *     用于独立行缓冲（每 SSBOType 一块 BDA 缓冲）的生命周期管理。
     * EN: Take buffer ownership: the destructor releases the DeviceBuffer.
     */
    void OwnBuffer(VkBufferOwner *buf)
    {
        SetBuffer(buf);
        owned_buffer = (buf != nullptr);
    }

    /**
     * CN: 返回此访问器对应的 SSBO 类型
     * EN: Return the SSBO type.
     */
    mtl::SSBOType GetSSBOType() const { return ssbo_type; }

    /**
     * CN: 返回最小 SSBO 身份（type + id），可直接传给 UpsertRecipeSSBOAssetBinding：
     *       UpsertRecipeSSBOAssetBinding(recipe, name, accessor->GetSSBOBinding());
     * EN: Return minimal SSBO identity (type + id) for use with UpsertRecipeSSBOAssetBinding.
     */
    mtl::SSBOBinding GetSSBOBinding() const { return {ssbo_type, ssbo_id}; }

    /**
     * CN: 返回元素数量
     * EN: Return element count
     */
    uint32_t GetCount() const { return element_count; }

    /**
     * CN: 获取底层缓冲区
     * EN: Get underlying buffer
     */
    VkBufferOwner* GetBuffer() { return buffer; }
    const VkBufferOwner* GetBuffer() const { return buffer; }

    /**
     * CN: 获取数组基址指针（用于批量操作）
     * EN: Get array base pointer (for bulk operations)
     */
    T* GetData() { return mapped_data; }
    const T* GetData() const { return mapped_data; }

    /**
     * CN: 下标访问 —— 返回对第 idx 个元素的引用
     *     修改后须调用 MarkDirty() + Commit()，或直接调用 Commit()
     * EN: Subscript access — returns reference to element at idx
     *     After modification, call MarkDirty() + Commit(), or just Commit()
     */
    T& operator[](uint32_t idx)
    {
        if (idx >= element_count)
            idx = element_count - 1; // 夹紧到末尾元素，避免越界崩溃

        if (stride_bytes)
            return *(T *)(reinterpret_cast<uint8_t *>(mapped_data) + size_t(idx) * stride_bytes);

        return mapped_data[idx];
    }

    const T& operator[](uint32_t idx) const
    {
        if (idx >= element_count)
            idx = element_count - 1;

        if (stride_bytes)
            return *(const T *)(reinterpret_cast<const uint8_t *>(mapped_data) + size_t(idx) * stride_bytes);

        return mapped_data[idx];
    }

    /**
     * CN: 标记整个缓冲区为 dirty（下次 Commit 时全量上传）
     * EN: Mark entire buffer dirty (full upload on next Commit)
     */
    void MarkDirty()
    {
        dirty = true;
        if (gpu_buf)
            gpu_buf->MarkDirty(0, static_cast<VkDeviceSize>(sizeof(T)) * element_count);
    }

    /**
     * CN: 检查是否有未提交的修改
     * EN: Check if there are uncommitted modifications
     */
    bool IsDirty() const { return dirty; }

    /**
     * CN: 提交修改到 GPU（仅当 dirty 时）
     * EN: Commit modifications to GPU (only when dirty)
     */
    void Commit() { CommitInternal(); }

    /**
     * CN: Update 接口（供 ECS BufferCommitQueue 驱动调用）
     * EN: Update hook for ECS BufferCommitQueue
     */
    void Update() const override
    {
        const_cast<SSBOArrayAccessor<T>*>(this)->CommitInternal();
    }

    /**
     * CN: 获取单个元素字节大小（编译期常量）
     * EN: Get single element byte size (compile-time constant)
     */
    static constexpr VkDeviceSize GetElementSize() { return sizeof(T); }

    /**
     * CN: 获取总缓冲区字节大小
     * EN: Get total buffer byte size
     */
    VkDeviceSize GetTotalSize() const
    {
        const VkDeviceSize stride = stride_bytes ? stride_bytes : sizeof(T);
        return stride * element_count;
    }

};//class SSBOArrayAccessor

}//namespace hgl::graph
