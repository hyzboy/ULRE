#pragma once

#include<hgl/vk/buffer/BufferView.h>
#include<hgl/mtl/MaterialRecipe.h>     ///< for mtl::SSBOType / mtl::SSBOBinding
#include<cassert>

namespace hgl::graph{

/**
 * 结构化缓冲区数组访问器
 *
 * CN: 将任意 C++ 结构体数组直接映射到 GPU SSBO 缓冲区，提供：
 * 1. 类型安全的 operator[] 元素访问（像访问普通 C++ 数组一样）
 * 2. 脏范围只交 L2（IGPUBuffer::MarkDirty），视图不自持 dirty
 * 3. 统一的 Commit 接口 = 把窗口范围标脏（兼容所有缓冲实现）
 * 4. 窗口（Map/Unmap 或外部内存）由 BufferView 统一管理
 * 5. 内置 SSBO ID 存储（由 SSBOBufferRegistry 分配）
 * 6. 元素窗口 (element_offset, element_count) + Bind 重绑：视图可指向 buffer 的任意段
 * 6. 不拥有数据源：buffer/gpu_buf 只是引用（行缓冲归 SSBOBufferRegistry；池窗口无 buffer）
 *
 * EN: Maps any C++ struct array directly to a GPU SSBO buffer, providing:
 * 1. Type-safe operator[] element access (like a normal C++ array)
 * 2. Automatic dirty tracking
 * 3. Unified Commit interface (compatible with all buffer types)
 * 4. Automatic Map/Unmap lifecycle management
 * 5. Built-in SSBO ID storage (assigned by SSBOBufferRegistry)
 *
 * 典型用途 / Typical usage:
 *   通用运行时结构化 SSBO 数组。
 *
 * 使用示例 / Usage Example:
 * ```cpp
 * // 通用运行时 SSBO：一步式创建，ID 自动分配并存储在 accessor 内
 * auto* acc = domain_manager->AllocateArrayAccessor<Color4f>(
 *     SSBOType::UserDefined, "MySSBO", DRAW_COUNT);
 *
 * // 材质 payload 行必须从 MaterialSSBOBufferRegistry 获取，并用
 * // accessor 的 ID 与显式 MaterialSSBOType 建立 recipe binding。
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
class ArrayView : public BufferView
{
private:
    uint32_t      element_count = 0;                           ///< 数组元素数量 / Element count
    uint32_t      ssbo_id       = 0;                           ///< 分配到的 SSBO ID（由 SSBOBufferRegistry 写入）
    mtl::SSBOType ssbo_type     = mtl::SSBOType::UserDefined;  ///< SSBO 类型（由 SSBOBufferRegistry 写入）
    uint32_t      stride_bytes  = 0;                           ///< 行距字节数（0=sizeof(T) 紧密排布；Arena 路径=sizeof(T) 且 16B 对齐）
    // 映射基址/范围/脏标记统一由 BufferView 的窗口机制持有：
    // 不再自持 mapped_data / dirty / host_direct（原先的 host_direct 即"外部窗口"）
    // 数据源的生命周期也不在本类：buffer/gpu_buf 只是引用（行缓冲归 SSBOBufferRegistry，
    // 池窗口连 buffer 都没有）——views never own their source。

    friend class VulkanDevice;
    friend class SSBOBufferRegistry;

private:

    // 元素窗口 = (element_offset, element_count)：窗口可以只是 buffer 的一段
    // （VAB/IBO 子分配、池行段窗口都靠它），窗口偏移记在基类 window_offset 里。
    explicit ArrayView(BufferOwner *buf, uint32_t element_offset, uint32_t count)
        : BufferView()
        , element_count(count)
    {
        SetBuffer(buf);

        if (element_count > 0)
            MapWindow(static_cast<VkDeviceSize>(element_offset) * sizeof(T),
                      static_cast<VkDeviceSize>(element_count) * sizeof(T));
    }

    explicit ArrayView(BufferOwner *buf, uint32_t count)
        : ArrayView(buf, 0, count)
    {
    }

    // Arena+BDA 路径：直写宿主窗口（外部窗口 = 行段基址，不经 Map/Unmap）
    explicit ArrayView(void *host_base, uint32_t count, uint32_t in_stride)
        : BufferView()
        , element_count(count)
        , stride_bytes(in_stride)
    {
        AttachWindow(host_base, static_cast<VkDeviceSize>(sizeof(T)) * element_count);
    }

    bool CommitInternal()
    {
        if (!gpu_buf)
            return false;       // 外部窗口（Arena 直写）：宿主内存即 GPU 可见，无需提交

        // 数据本就在映射窗口里，只需把脏范围交给 L2。
        // 原先那句 gpu_buf->Write(mapped_data, 0, size) 是"映射区拷回映射区"的自我 memcpy
        // （StagedBuffer::Write = memcpy 到 staging + MarkDirty），去掉后语义不变。
        MarkWindowDirty();
        return true;
    }

public:

    ArrayView() = default;

    /**
     * CN: 工厂方法 —— 从已有 BufferOwner 创建数组访问器
     * EN: Factory — create from an existing BufferOwner
     *
     * @param buf   已创建的 GPU 缓冲区（大小须 >= sizeof(T) * count）
     * @param count 数组元素数量
     */
    static ArrayView* Create(BufferOwner *buf, uint32_t count)
    {
        if (!buf || count == 0)
            return nullptr;

        return new ArrayView(buf, count);
    }

    ~ArrayView()
    {
        UnmapWindow();          // 只解自己的窗口：数据源不归视图，不在此释放

        buffer  = nullptr;
        gpu_buf = nullptr;
    }

    /**
     * CN: 重绑数据源与元素窗口（视图可反复指向不同 buffer/段，不拥有数据源）
     * EN: Rebind source and element window (the view owns nothing).
     */
    void Bind(BufferOwner *buf, uint32_t element_offset = 0, uint32_t count = 0)
    {
        element_count = count;
        SetBuffer(buf);         // 换源会解除旧窗口（外部窗口除外）

        if (element_count > 0)
            MapWindow(static_cast<VkDeviceSize>(element_offset) * sizeof(T),
                      static_cast<VkDeviceSize>(element_count) * sizeof(T));
    }

    // 禁止拷贝 / Disable copy
    ArrayView(const ArrayView&) = delete;
    ArrayView& operator=(const ArrayView&) = delete;

    // 允许移动 / Allow move
    ArrayView(ArrayView&& other) noexcept
        : BufferView()
        , element_count(other.element_count)
        , ssbo_id(other.ssbo_id)
        , ssbo_type(other.ssbo_type)
    {
        MoveFrom(std::move(other));     // 窗口（基址/范围/外部标志）随 MoveFrom 一起搬
        other.element_count = 0;
        other.ssbo_id       = 0;
        other.ssbo_type     = mtl::SSBOType::UserDefined;
    }

    ArrayView& operator=(ArrayView&& other) noexcept
    {
        if (this != &other)
        {
            UnmapWindow();
            SetBuffer(nullptr);

            MoveFrom(std::move(other));
            element_count = other.element_count;
            ssbo_id       = other.ssbo_id;
            ssbo_type     = other.ssbo_type;

            other.element_count = 0;
            other.ssbo_id       = 0;
            other.ssbo_type     = mtl::SSBOType::UserDefined;
        }
        return *this;
    }

public:

    /**
     * CN: 检查是否有效（已映射且元素数 > 0）
     * EN: Check if valid (mapped and non-empty)
     */
    bool IsValid() const { return HasWindow() && element_count > 0; }
    operator bool() const { return IsValid(); }

    /**
     * CN: 返回此访问器对应的 SSBO ID（由 SSBOBufferRegistry 分配时写入）
     * EN: Return the SSBO ID assigned by SSBOBufferRegistry.
     */
    uint32_t GetSSBOId() const { return ssbo_id; }

    /**
     * CN: 返回此访问器对应的 SSBO 类型
     * EN: Return the SSBO type.
     */
    mtl::SSBOType GetSSBOType() const { return ssbo_type; }

    /**
     * CN: 返回通用运行时 SSBO 身份（type + id）。材质 payload recipe 绑定必须
     *     单独提供显式 MaterialSSBOType，不能使用该返回值推导类型。
     * EN: Return the generic runtime SSBO identity (type + id).
     */
    mtl::SSBOBinding GetSSBOBinding() const { return {ssbo_type, ssbo_id}; }

    /**
     * CN: 返回元素数量
     * EN: Return element count
     */
    uint32_t GetCount() const { return element_count; }

    /**
     * CN: 元素窗口在 buffer 内的起始元素号（窗口偏移 / sizeof(T)；
     *     外部窗口（Arena/池行段）恒为 0）
     * EN: First element index of the window inside the buffer.
     */
    uint32_t GetElementOffset() const
    {
        return sizeof(T) ? uint32_t(GetWindowOffset() / sizeof(T)) : 0;
    }

    /**
     * CN: 获取底层缓冲区
     * EN: Get underlying buffer
     */
    BufferOwner* GetBuffer() { return buffer; }
    const BufferOwner* GetBuffer() const { return buffer; }

    /**
     * CN: 获取数组基址指针（用于批量操作）
     * EN: Get array base pointer (for bulk operations)
     */
    T* GetData() { return static_cast<T*>(GetWindowData()); }
    const T* GetData() const { return static_cast<const T*>(GetWindowData()); }

    /**
     * CN: 下标访问 —— 返回对第 idx 个元素的引用
     *     修改后须调用 MarkDirty() + Commit()，或直接调用 Commit()
     * EN: Subscript access — returns reference to element at idx
     *     After modification, call MarkDirty() + Commit(), or just Commit()
     */
    T& operator[](uint32_t idx)
    {
        assert(idx < element_count && "ArrayView: index out of range (project bug)");
        if (idx >= element_count)
            idx = element_count - 1;    // Release 兜底：宁读末行也不越界写

        if (stride_bytes)
            return *(T *)(reinterpret_cast<uint8_t *>(GetWindowData()) + size_t(idx) * stride_bytes);

        return static_cast<T *>(GetWindowData())[idx];
    }

    const T& operator[](uint32_t idx) const
    {
        assert(idx < element_count && "ArrayView: index out of range (project bug)");
        if (idx >= element_count)
            idx = element_count - 1;

        if (stride_bytes)
            return *(const T *)(reinterpret_cast<const uint8_t *>(GetWindowData()) + size_t(idx) * stride_bytes);

        return static_cast<const T *>(GetWindowData())[idx];
    }

    /**
     * CN: 标记整个缓冲区为 dirty（下次 Commit 时全量上传）
     * EN: Mark entire buffer dirty (full upload on next Commit)
     */
    void MarkDirty()
    {
        // 脏范围交 L2（窗口 = 整个数组；staging/显存由 L2 决定），视图不自持 dirty
        MarkWindowDirty();
    }

    /**
     * CN: 检查是否有未提交的修改
     * EN: Check if there are uncommitted modifications
     */
    bool IsDirty() const { return gpu_buf ? gpu_buf->IsDirty() : false; }

    /**
     * CN: 提交修改到 GPU（仅当 dirty 时）
     * EN: Commit modifications to GPU (marks the window range dirty on L2)
     */
    void Commit() { CommitInternal(); }

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

};//class ArrayView

}//namespace hgl::graph
