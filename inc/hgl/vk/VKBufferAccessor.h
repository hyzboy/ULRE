#pragma once

#include<hgl/vk/VKVertexAttribBuffer.h>
#include<hgl/vk/VKIndexBuffer.h>
#include<hgl/vk/VertexAttribDataAccess.h>
#include<hgl/vk/VKBufferAccessBase.h>

namespace hgl::graph{

/**
 * åå§ç±»åæ°æ®è®¿é®å¨ï¼ååç´ ç±»åï¼
 * CN: ç¨äºåéé VBO å?IndexBuffer çè½»éçº§åè£å?
 * EN: Lightweight wrapper for single-channel VBO and IndexBuffer
 *
 * æä¾ä¸?VertexAttribDataAccess å¼å®¹çæ¥å£ï¼ä½ç´æ¥è¿ååå§æé?
 */
template<typename T>
class RawDataAccess
{
public:
    using value_type = T;  // ç±»åå«åï¼ç¨äºæ¨¡æ¿åç¼ç¨

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

    // ç´æ¥è®¿é®æé
    T* Get() { return data; }
    const T* Get() const { return data; }
    operator T*() { return data; }
    operator const T*() const { return data; }

    T& operator[](size_t index) { return data[index]; }
    const T& operator[](size_t index) const { return data[index]; }
};



/**
 * ç»ä¸ç?Buffer è®¿é®å?(èå°è£çæ?
 *
 * CN: æ ¸å¿è®¾è®¡çå¿µï¼?
 * 1. ç»ä¸è®¿é®æ¥å£ - ä¸ç®¡åºå±æ?CPUOnlyãReBARãStagedBuffer è¿æ¯ RingBuffer
 * 2. èªå¨ Map/Unmap - RAII ç®¡çï¼èªå¨çå½å¨æ?
 * 3. Dirtyè¿½è¸ª - åå¥èªå¨æ è®°ï¼Commitæ¶æä¸ä¼ 
 * 4. é¶ä¸­é´å± - ç´æ¥æä½ VABï¼ä¸åéè¦?VABMap
 *
 * EN: Core design principles:
 * 1. Unified access interface - regardless of CPUOnly, ReBAR, StagedBuffer or RingBuffer
 * 2. Auto Map/Unmap - RAII management, automatic lifecycle
 * 3. Dirty tracking - writes auto-mark, upload only on Commit
 * 4. Zero intermediate layers - direct VAB access, no VABMap needed
 *
 * ä½¿ç¨ç¤ºä¾ / Usage:
 * ```cpp
 * BufferAccessor<VB3f> positions(vab);
 * positions->Write(vec);      // èªå¨æ è®° dirty
 * positions.Commit();         // æé flush å?GPU
 * ```
 */
template<typename DataAccessType>
class BufferAccessor:public BufferAccessBase
{
private:
    uint32_t buffer_total_count;        ///< æ»åç´ æ°é?/ Total element count
    uint32_t buffer_stride;             ///< ååç´ å­èæ° / Stride in bytes
    DataAccessType *data_access;        ///< æ°æ®è®¿é®å?/ Data accessor
    void *mapped_pointer;               ///< æ å°æé / Mapped pointer
    int32_t element_offset;             ///< é¡¶ç¹åç§»(åä½:åç´ ) / Element offset
    uint32_t element_count;             ///< è®¿é®åç´ æ°é / Element count

    /**
     * CN: åé¨ Map æä½
     * EN: Internal map operation
     */
    void MapInternal()
    {
        if(!buffer || mapped_pointer)
            return;

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

        mapped_pointer = buffer->Map(static_cast<VkDeviceSize>(element_offset), count);
        if(mapped_pointer)
        {
            data_access = DataAccessType::Create(count, mapped_pointer);
            if(data_access)
                data_access->Begin();
        }
    }

    /**
     * CN: åé¨ Unmap æä½
     * EN: Internal unmap operation
     */
    void UnmapInternal()
    {
        if(data_access)
        {
            delete data_access;
            data_access = nullptr;
        }

        if(buffer && mapped_pointer)
        {
            buffer->Unmap();
            mapped_pointer = nullptr;
        }
    }

public:

    /**
     * CN: æé å½æ?
     * EN: Constructor
     * \param vab Bufferæé / Buffer pointer
     * \param take_ownership æ¯å¦è·åæææ / Take ownership
     */
    BufferAccessor(VAB *vab = nullptr, bool take_ownership = false)
        : BufferAccessBase()
        , buffer_total_count(vab ? vab->GetCount() : 0)
        , buffer_stride(vab ? vab->GetStride() : 0)
        , data_access(nullptr)
        , mapped_pointer(nullptr)
        , element_offset(0)
        , element_count(0)
    {
        SetBuffer(vab, take_ownership);
        if(buffer)
            MapInternal();
    }

    BufferAccessor(VAB *vab, int32_t offset, uint32_t count, bool take_ownership = false)
        : BufferAccessBase()
        , buffer_total_count(vab ? vab->GetCount() : 0)
        , buffer_stride(vab ? vab->GetStride() : 0)
        , data_access(nullptr)
        , mapped_pointer(nullptr)
        , element_offset(offset)
        , element_count(count)
    {
        SetBuffer(vab, take_ownership);
        if(buffer)
            MapInternal();
    }

    BufferAccessor(IndexBuffer *ibo, int32_t offset, uint32_t count, bool take_ownership = false)
        : BufferAccessBase()
        , buffer_total_count(ibo ? ibo->GetCount() : 0)
        , buffer_stride(ibo ? ibo->GetStride() : 0)
        , data_access(nullptr)
        , mapped_pointer(nullptr)
        , element_offset(offset)
        , element_count(count)
    {
        SetBuffer(ibo, take_ownership);
        if(buffer)
            MapInternal();
    }

    /**
     * CN: ææå½æ° - èªå¨ Unmap
     * EN: Destructor - Auto unmap
     */
    ~BufferAccessor()
    {
        UnmapInternal();
    }

    // ç¦æ­¢æ·è´ / Disable copy
    BufferAccessor(const BufferAccessor&) = delete;
    BufferAccessor& operator=(const BufferAccessor&) = delete;

    /**
     * CN: ç»å®å°æ°ç?buffer
     * EN: Bind to new buffer
     * \param vab Bufferæé / Buffer pointer
     * \param take_ownership æ¯å¦è·åæææ / Take ownership
     */
    void Bind(VAB *vab, int32_t offset = 0, uint32_t count = 0, bool take_ownership = false)
    {
        UnmapInternal();
        SetBuffer(vab, take_ownership);
        buffer_total_count = vab ? vab->GetCount() : 0;
        buffer_stride = vab ? vab->GetStride() : 0;
        element_offset = offset;
        element_count = count;

        if(buffer)
            MapInternal();
    }

    /**
     * CN: æ£æ¥æ¯å¦ææ?
     * EN: Check if valid
     */
    bool IsValid() const { return buffer && data_access; }
    operator bool() const { return IsValid(); }

    /**
     * CN: è·ååºå± buffer
     * EN: Get underlying buffer
     */
    VAB* GetBuffer() { return static_cast<VAB*>(buffer); }
    const VAB* GetBuffer() const { return static_cast<const VAB*>(buffer); }

    /**
     * CN: è·åæ°æ®è®¿é®å?
     * EN: Get data accessor
     */
    DataAccessType* Get() { return data_access; }
    const DataAccessType* Get() const { return data_access; }

    /**
     * CN: ç®­å¤´æä½ç¬?- ç´æ¥è®¿é®æ°æ®è®¿é®å?
     * EN: Arrow operator - direct data accessor access
     */
    DataAccessType* operator->() { return data_access; }
    const DataAccessType* operator->() const { return data_access; }

    /**
     * CN: éå¼è½¬æ¢ä¸ºåå§æé?(éè¿ DataAccessType çoperator T*())
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
     * CN: æ è®°ä¸?dirty
     * EN: Mark as dirty
     */
    void MarkDirty() { dirty = true; }

    /**
     * CN: æ£æ¥æ¯å?dirty
     * EN: Check if dirty
     */
    bool IsDirty() const { return dirty; }

    /**
     * CN: æäº¤ä¿®æ¹å?GPU
     *     å¯¹äº StagedBuffer: Unmap è§¦å flush
     *     å¯¹äº ReBAR/CPUOnly: æ æä½?å·²ç»åæ­¥)
     * EN: Commit changes to GPU
     *     For StagedBuffer: Unmap triggers flush
     *     For ReBAR/CPUOnly: No-op (already synced)
     * \return æ¯å¦æ§è¡äºæäº?/ Whether committed
     */
    bool Commit()
    {
        if(!dirty || !buffer)
            return false;

        // Unmap + Remap è§¦å flush (å¯¹äº StagedBuffer)
        UnmapInternal();
        MapInternal();

        dirty = false;

        // Reset frame counter after successful commit
        if(buffer)
            buffer->ResetFramesSinceUpdate();

        return true;
    }

    void Update() const override
    {
        const_cast<BufferAccessor*>(this)->Commit();
    }

    /**
     * CN: åè£ Write æ¹æ³ï¼èªå¨æ è®?dirty
     * EN: Wrapped Write method, auto-mark dirty
     */
    template<typename T>
    bool Write(const T& value)
    {
        if(!data_access)
            return false;

        bool result = data_access->Write(value);
        if(result)
            dirty = true;
        return result;
    }

    /**
     * CN: æ¹éåå¥ (ä½¿ç¨åºå± buffer ç?Write)
     * EN: Bulk write (using underlying buffer Write)
     */
    bool WriteBulk(const void *data, uint32_t element_count)
    {
        if(!buffer || !data || element_count == 0)
            return false;

        if(element_offset < 0)
            element_offset = 0;

        const uint32_t total = buffer_total_count;
        if(static_cast<uint32_t>(element_offset) >= total)
            return false;

        const uint32_t max_count = (this->element_count == 0)
            ? (total - static_cast<uint32_t>(element_offset))
            : this->element_count;

        if(element_count > max_count)
            return false;

        bool result = buffer->Write(data, static_cast<uint32_t>(element_offset), element_count);
        if(result)
            dirty = true;
        return result;
    }

    /**
     * CN: æ¹éè¯»å
     * EN: Bulk read
     */
    bool ReadBulk(void *dst, uint32_t element_count)
    {
        if(!buffer || !dst || element_count == 0 || !mapped_pointer)
            return false;

        if(buffer_stride == 0)
            return false;

        memcpy(dst, mapped_pointer, static_cast<size_t>(buffer_stride) * element_count);
        return true;
    }

    /**
     * CN: Seek å°æå®ä½ç½?
     * EN: Seek to position
     */
    bool Seek(size_t offset)
    {
        if(!data_access)
            return false;
        return data_access->Seek(offset);
    }

    /**
     * CN: éç½®å°å¼å§ä½ç½?
     * EN: Reset to beginning
     */
    void Begin()
    {
        if(data_access)
            data_access->Begin();
    }
};

// å¸¸ç¨ç±»åå®ä¹ / Common type definitions
using BufferAccessor1u8  = BufferAccessor<VB1u8>;
using BufferAccessor1i8  = BufferAccessor<VB1i8>;
using BufferAccessor2u8  = BufferAccessor<VB2u8>;
using BufferAccessor2f   = BufferAccessor<VB2f>;
using BufferAccessor3f   = BufferAccessor<VB3f>;
using BufferAccessor4f   = BufferAccessor<VB4f>;
using BufferAccessor2i   = BufferAccessor<VB2i>;
using BufferAccessor3i   = BufferAccessor<VB3i>;
using BufferAccessor4i   = BufferAccessor<VB4i>;

// ååå¼å®¹å«å / Backward compatibility aliases
// æ¨èéæ­¥è¿ç§»å?BufferAccessorï¼ä½æ§ä»£ç å¯ä»¥ç»§ç»­ä½¿ç¨è¿äºå«å?
using StagedVB1u8  = BufferAccessor1u8;
using StagedVB1i8  = BufferAccessor1i8;
using StagedVB2u8  = BufferAccessor2u8;
using StagedVB2f   = BufferAccessor2f;
using StagedVB3f   = BufferAccessor3f;
using StagedVB4f   = BufferAccessor4f;
using StagedVB2i   = BufferAccessor2i;
using StagedVB3i   = BufferAccessor3i;
using StagedVB4i   = BufferAccessor4i;

// IndexBuffer è®¿é®å¨ï¼ä½¿ç¨ BufferAccessor + RawDataAccessï¼?
using IndexAccessorU8  = BufferAccessor<RawDataAccess<uint8>>;
using IndexAccessorU16 = BufferAccessor<RawDataAccess<uint16>>;
using IndexAccessorU32 = BufferAccessor<RawDataAccess<uint32>>;

// åééåå§ç±»åè®¿é®å¨ï¼ç¨äºåéé VBOï¼?
using RawAccessorU8    = BufferAccessor<RawDataAccess<uint8>>;
using RawAccessorI8    = BufferAccessor<RawDataAccess<int8>>;
using RawAccessorU16   = BufferAccessor<RawDataAccess<uint16>>;
using RawAccessorI16   = BufferAccessor<RawDataAccess<int16>>;
using RawAccessorU32   = BufferAccessor<RawDataAccess<uint32>>;
using RawAccessorI32   = BufferAccessor<RawDataAccess<int32>>;
using RawAccessorFloat = BufferAccessor<RawDataAccess<float>>;

}//namespace hgl::graph
