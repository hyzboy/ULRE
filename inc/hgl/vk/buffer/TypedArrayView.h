#pragma once

#include<hgl/vk/buffer/VertexAttribBuffer.h>
#include<hgl/vk/buffer/IndexBuffer.h>
#include<hgl/vk/VertexAttribDataAccess.h>
#include<hgl/vk/buffer/BufferView.h>
#include<hgl/vk/VKDevice.h>

namespace hgl::graph{

/**
 * 原始类型数据访问器（单元素类型）
 * CN: 用于单通道 VBO 和 IndexBuffer 的轻量级包装器
 * EN: Lightweight wrapper for single-channel VBO and IndexBuffer
 *
 * 提供与 VertexAttribDataAccess 兼容的接口，但直接返回原始指针
 */
template<typename T>
class RawDataAccess
{
public:
    using value_type = T;  // 类型别名，用于模板元编程

private:
    T *data;
    T *data_end;
    T *access;
    size_t count;

public:
    RawDataAccess(size_t _count, T *_data)
        : data(_data)
        , count(_count)
        , access(nullptr)
    {
        if(data && count > 0)
            data_end = data + count;
        else
            data_end = nullptr;
    }

    virtual ~RawDataAccess() = default;

    static RawDataAccess<T>* Create(size_t count, void *ptr)
    {
        return new RawDataAccess<T>(count, static_cast<T*>(ptr));
    }

    bool IsValid() const { return data != nullptr; }

    T* Begin(size_t offset = 0)
    {
        if(!data)
            return nullptr;

        access = (offset < count) ? (data + offset) : data;
        return access;
    }

    void End()
    {
        access = nullptr;
    }

    bool Seek(size_t offset)
    {
        if(!data || offset >= count)
            return false;

        access = data + offset;
        return true;
    }

    bool Write(const T& value)
    {
        if(!access || access >= data_end)
            return false;

        *access++ = value;
        return true;
    }

    bool WriteData(const T *ptr, uint32_t number)
    {
        if(!access || access + number > data_end || !ptr)
            return false;

        memcpy(access, ptr, number * sizeof(T));
        access += number;
        return true;
    }

    // 直接访问指针
    T* Get() { return data; }
    const T* Get() const { return data; }
    operator T*() { return data; }
    operator const T*() const { return data; }

    T& operator[](size_t index) { return data[index]; }
    const T& operator[](size_t index) const { return data[index]; }
};



/**
 * 统一的 Buffer 访问器（薄封装版本）
 *
 * CN: 核心设计理念：
 * 1. 统一访问接口 - 不管底层是 CPUOnly、ReBAR、StagedBuffer 还是 RingBuffer
 * 2. 自动 Map/Unmap - RAII 管理，自动生命周期
 * 3. Dirty 追踪 - 写入自动标记，Commit 时才上传
 * 4. 零中间层 - 直接操作 VAB，不再需要 VABMap
 *
 * EN: Core design principles:
 * 1. Unified access interface - regardless of CPUOnly, ReBAR, StagedBuffer or RingBuffer
 * 2. Auto Map/Unmap - RAII management, automatic lifecycle
 * 3. Dirty tracking - writes auto-mark, upload only on Commit
 * 4. Zero intermediate layers - direct VAB access, no VABMap needed
 *
 * 使用示例 / Usage:
 * ```cpp
 * TypedArrayView<VB3f> positions(vab);
 * positions->Write(vec);      // 自动标记 dirty
 * positions.Commit();         // 按需 flush 到 GPU
 * ```
 */
template<typename DataAccessType>
class TypedArrayView:public BufferView
{
private:
     uint32_t buffer_total_count;        ///< 总元素数量 / Total element count
     uint32_t buffer_stride;             ///< 单元素字节数 / Stride in bytes
     DataAccessType *data_access;        ///< 数据访问器 / Data accessor
     int32_t element_offset;             ///< 元素偏移(单位:元素) / Element offset
     uint32_t element_count;             ///< 访问元素数量 / Element count
     // 映射基址与脏标记统一由 BufferView 的窗口机制持有：
     // 窗口 = (element_offset*stride, count*stride)，不再自持 mapped_pointer / dirty


    /**
     * CN: 内部 Map 操作
     * EN: Internal map operation
     */
    void MapInternal()
    {
        if(element_offset < 0)
            element_offset = 0;

        uint32_t count = element_count;
        const uint32_t total = buffer_total_count;

        if(static_cast<uint32_t>(element_offset) >= total)
            return;

        if(count == 0)
            count = total - static_cast<uint32_t>(element_offset);

        if(count == 0)
            return;

        // Byte-offset Map: buffer_stride is element size, multiply to get byte offsets.
        // 窗口状态由 BufferView 保存（同一窗口幂等）。
        if(!MapWindow(static_cast<VkDeviceSize>(element_offset) * buffer_stride,
                      static_cast<VkDeviceSize>(count) * buffer_stride))
            return;

        data_access = DataAccessType::Create(count, GetWindowData());
        if(data_access)
            data_access->Begin();
    }

    /**
     * CN: 内部 Unmap 操作
     * EN: Internal unmap operation
     */
    void UnmapInternal()
    {
        if(data_access)
        {
            delete data_access;
            data_access = nullptr;
        }

        UnmapWindow();
    }

public:

     /**
      * CN: 构造函数
      * EN: Constructor
      * \param vab Buffer 指针 / Buffer pointer
      */
    TypedArrayView(VAB *vab = nullptr)
        : BufferView()
        , buffer_total_count(vab ? vab->GetCount() : 0)
        , buffer_stride(vab ? vab->GetStride() : 0)
        , data_access(nullptr)
        , element_offset(0)
        , element_count(0)
    {
        SetBuffer(vab);
        if(gpu_buf)
            MapInternal();
    }

    TypedArrayView(VAB *vab, int32_t offset, uint32_t count)
        : BufferView()
        , buffer_total_count(vab ? vab->GetCount() : 0)
        , buffer_stride(vab ? vab->GetStride() : 0)
        , data_access(nullptr)
        , element_offset(offset)
        , element_count(count)
    {
        SetBuffer(vab);
        if(gpu_buf)
            MapInternal();
    }

    TypedArrayView(IndexBuffer *ibo, int32_t offset, uint32_t count)
        : BufferView()
        , buffer_total_count(ibo ? ibo->GetCount() : 0)
        , buffer_stride(ibo ? ibo->GetStride() : 0)
        , data_access(nullptr)
        , element_offset(offset)
        , element_count(count)
    {
        SetBuffer(ibo);
        if(gpu_buf)
            MapInternal();
    }

    /**
     * CN: 析构函数 - 自动 Unmap
     * EN: Destructor - Auto unmap
     */
    ~TypedArrayView()
    {
        UnmapInternal();
    }

    // 禁止拷贝 / Disable copy
    TypedArrayView(const TypedArrayView&) = delete;
    TypedArrayView& operator=(const TypedArrayView&) = delete;

     /**
      * CN: 绑定到新的 buffer
      * EN: Bind to new buffer
      * \param vab Buffer 指针 / Buffer pointer
      */
    void Bind(VAB *vab, int32_t offset = 0, uint32_t count = 0)
    {
        UnmapInternal();
        SetBuffer(vab);
        buffer_total_count = vab ? vab->GetCount() : 0;
        buffer_stride = vab ? vab->GetStride() : 0;
        element_offset = offset;
        element_count = count;

        if(gpu_buf)
            MapInternal();
    }

    /**
     * CN: 检查是否有效
     * EN: Check if valid
     */
    bool IsValid() const { return HasWindow() && data_access; }
    operator bool() const { return IsValid(); }

    /**
     * CN: 获取数据访问器
     * EN: Get data accessor
     */
    DataAccessType* Get() { return data_access; }
    const DataAccessType* Get() const { return data_access; }

    /**
     * CN: 箭头操作符 - 直接访问数据访问器
     * EN: Arrow operator - direct data accessor access
     */
    DataAccessType* operator->() { return data_access; }
    const DataAccessType* operator->() const { return data_access; }

    /**
     * CN: 隐式转换为原始指针（通过 DataAccessType 的 operator T*()）
     * EN: Implicit conversion to raw pointer (via DataAccessType's operator T*())
     */
    template<typename T = DataAccessType>
    operator typename T::value_type*()
    {
        return data_access ? static_cast<typename T::value_type*>(*data_access) : nullptr;
    }

    template<typename T = DataAccessType>
    operator const typename T::value_type*() const
    {
        return data_access ? static_cast<const typename T::value_type*>(*data_access) : nullptr;
    }

    /**
     * CN: 标记为 dirty
     * EN: Mark as dirty
     */
    void MarkDirty()
    {
        // MapInternal 时窗口就是本视图的元素范围，直接把窗口标脏交 L2
        // （视图不自持 dirty，消除与 L2 的双记账）
        MarkWindowDirty();
    }

    /**
     * CN: 检查是否 dirty
     * EN: Check if dirty
     */
    bool IsDirty() const { return gpu_buf ? gpu_buf->IsDirty() : false; }

    /**
     * CN: 包装 Write 方法，自动标记 dirty
     * EN: Wrapped Write method, auto-mark dirty
     */
    template<typename T>
    bool Write(const T& value)
    {
        if(!data_access)
            return false;

        bool result = data_access->Write(value);
        if(result)
            MarkDirty();
        return result;
    }

    /**
     * CN: Seek 到指定位置
     * EN: Seek to position
     */
    bool Seek(size_t offset)
    {
        if(!data_access)
            return false;
        return data_access->Seek(offset);
    }

    /**
     * CN: 重置到开始位置
     * EN: Reset to beginning
     */
    void Begin()
    {
        if(data_access)
            data_access->Begin();
    }
};

// 常用类型定义 / Common type definitions
using TypedArrayView1u8  = TypedArrayView<VB1u8>;
using TypedArrayView1i8  = TypedArrayView<VB1i8>;
using TypedArrayView2u8  = TypedArrayView<VB2u8>;
using TypedArrayView2f   = TypedArrayView<VB2f>;
using TypedArrayView2hf  = TypedArrayView<VB2hf>;
using TypedArrayView3f   = TypedArrayView<VB3f>;
using TypedArrayView4f   = TypedArrayView<VB4f>;
using TypedArrayView2i   = TypedArrayView<VB2i>;
using TypedArrayView3i   = TypedArrayView<VB3i>;
using TypedArrayView4i   = TypedArrayView<VB4i>;
using TypedArrayView2i16 = TypedArrayView<VB2i16>;   // RG16i 位置（int16 raw——2D 压缩）
using TypedArrayView2u16 = TypedArrayView<VB2u16>;   // RG16UI 位置（uint16 raw——2D 压缩）

// IndexBuffer 访问器（使用 TypedArrayView + RawDataAccess）
using IndexAccessorU8  = TypedArrayView<RawDataAccess<uint8>>;
using IndexAccessorU16 = TypedArrayView<RawDataAccess<uint16>>;
using IndexAccessorU32 = TypedArrayView<RawDataAccess<uint32>>;

// 单通道原始类型访问器（用于单通道 VBO）
using RawAccessorU8    = TypedArrayView<RawDataAccess<uint8>>;
using RawAccessorI8    = TypedArrayView<RawDataAccess<int8>>;
using RawAccessorU16   = TypedArrayView<RawDataAccess<uint16>>;
using RawAccessorI16   = TypedArrayView<RawDataAccess<int16>>;
using RawAccessorU32   = TypedArrayView<RawDataAccess<uint32>>;
using RawAccessorI32   = TypedArrayView<RawDataAccess<int32>>;
using RawAccessorFloat = TypedArrayView<RawDataAccess<float>>;

}//namespace hgl::graph
