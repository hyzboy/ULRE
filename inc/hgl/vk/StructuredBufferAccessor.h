#pragma once

#include<hgl/vk/VKBufferAccessBase.h>

#include<type_traits>

VK_NAMESPACE_BEGIN

/**
 * ç»æåç¼å²åºè®¿é®å?
 *
 * CN: å°ä»»æ?C++ ç»æä½æç±»ç´æ¥æ å°å° GPU ç¼å²åºï¼æä¾ï¼?
 * 1. ä¾¿æ·ç?CPU ç«¯æ°æ®ä¿®æ¹ï¼å°±åè®¿é®æ®é?C++ å¯¹è±¡ä¸æ ·ï¼
 * 2. èªå¨ dirty è¿½è¸ª
 * 3. ç»ä¸ç?Commit æ¥å£ï¼å¼å®¹ææç¼å²åºç±»åï¼GPUOnly, ReBAR, StagedBuffer, RingBufferï¼?
 * 4. èªå¨ Map/Unmap çå½å¨æç®¡ç
 *
 * EN: Maps any C++ struct or class directly to GPU buffer, provides:
 * 1. Convenient CPU-side data modification (like accessing normal C++ objects)
 * 2. Automatic dirty tracking
 * 3. Unified Commit interface (compatible with all buffer types)
 * 4. Automatic Map/Unmap lifecycle management
 *
 * ä½¿ç¨ç¤ºä¾ / Usage Example:
 * ```cpp
 * struct CameraData { glm::mat4 vp; glm::vec3 pos; };
 * auto camera_buf = device->CreateUBO(sizeof(CameraData));
 *
 * StructuredBufferAccessor<CameraData> cam_accessor(camera_buf);
 *
 * // ç´æ¥ä¿®æ¹ CPU ç«¯æ°æ?/ Modify CPU-side data directly
 * cam_accessor.Data()->vp = glm::mat4(1.0f);
 * cam_accessor.Data()->pos = glm::vec3(0, 0, 10);
 *
 * // èªå¨æ è®° dirty å¹¶æäº¤å° GPU
 * cam_accessor.Commit();  // åé¨èªå¨è°ç¨ Flush å¦æéè¦?
 * ```
 */
template<typename T>
class StructuredBufferAccessor:public BufferAccessBase
{
private:
    T *mapped_data;                 ///< æ å°åçæ°æ®æé / Mapped data pointer
    VkDeviceSize aligned_size = 0;
    bool initialized = false;
    friend class VulkanDevice;

    /**
     * CN: åé¨ Map æä½
     * EN: Internal map operation
     */
    void MapInternal()
    {
        if(!buffer || mapped_data)
            return;

        void *ptr = buffer->Map(0, aligned_size);
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
        SetBuffer(buf, take_ownership);
        if(buffer)
            MapInternal();
        InitDefaultsIfNeeded();
    }

    /**
     * CN: åé¨ Unmap æä½
     * EN: Internal unmap operation
     */
    void UnmapInternal()
    {
        if(!buffer || !mapped_data)
            return;

        buffer->Unmap();
        mapped_data = nullptr;
    }

    StructuredBufferAccessor(DeviceBuffer *buf, bool take_ownership = false)
        : BufferAccessBase()
        , mapped_data(nullptr)
        , aligned_size(buf ? buf->GetSize() : 0)
    {
        SetBuffer(buf, take_ownership);
        if(buffer)
            MapInternal();
        InitDefaultsIfNeeded();
    }

    StructuredBufferAccessor(DeviceBuffer *buf, DescriptorSetType dst, const AnsiString &name, bool take_ownership = false)
        : BufferAccessBase()
        , mapped_data(nullptr)
        , aligned_size(buf ? buf->GetSize() : 0)
    {
        SetBuffer(buf, take_ownership);
        SetUBOMeta(dst, name);
        if(buffer)
            MapInternal();
        InitDefaultsIfNeeded();
    }

    StructuredBufferAccessor(DeviceBuffer *buf, const ShaderBufferDesc *desc, bool take_ownership = false)
        : BufferAccessBase()
        , mapped_data(nullptr)
        , aligned_size(buf ? buf->GetSize() : 0)
    {
        SetBuffer(buf, take_ownership);
        SetUBOMeta(desc ? desc->set_type : DescriptorSetType::PerMaterial, desc ? desc->name : "");
        if(buffer)
            MapInternal();
        InitDefaultsIfNeeded();
    }

public:
    /**
     * CN: ææå½æ° - èªå¨ Unmap åå¯éç buffer å é¤
     * EN: Destructor - auto unmap and optional buffer cleanup
     */
    ~StructuredBufferAccessor()
    {
        UnmapInternal();
    }

    // ç¦æ­¢æ·è´ / Disable copy
    StructuredBufferAccessor(const StructuredBufferAccessor&) = delete;
    StructuredBufferAccessor& operator=(const StructuredBufferAccessor&) = delete;

    // åè®¸ç§»å¨ / Allow move
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

private:

    /**
     * CN: ç»å®å°æ°çç¼å²åº
     * EN: Bind to new buffer
     */
    void Bind(DeviceBuffer *buf, bool take_ownership = false)
    {
        UnmapInternal();
        aligned_size = buf ? buf->GetSize() : 0;
        SetBuffer(buf, take_ownership);

        if(buffer)
            MapInternal();
    }

public:

    /**
     * CN: æ£æ¥æ¯å¦ææ?
     * EN: Check if valid
     */
    bool IsValid() const { return buffer && mapped_data; }
    operator bool() const { return IsValid(); }

    /**
     * CN: è·ååºå±ç¼å²å?
     * EN: Get underlying buffer
     */
    DeviceBuffer* GetBuffer() { return buffer; }
    const DeviceBuffer* GetBuffer() const { return buffer; }

    /**
     * CN: è·åç»æä½æ°æ®æé?
     * ä¿®æ¹æ­¤æéæåçæ°æ®åï¼éè°ç¨ MarkDirty() æ?Commit()
     * EN: Get struct data pointer
     * After modifying data through this pointer, call MarkDirty() or Commit()
     */
    T* Data() { return mapped_data; }
    const T* Data() const { return mapped_data; }

    /**
     * CN: ç®­å¤´æä½ç¬?- ç´æ¥è®¿é®ç»æä½æå?
     * EN: Arrow operator - direct struct member access
     */
    T* operator->() { return mapped_data; }
    const T* operator->() const { return mapped_data; }

    /**
     * CN: è§£å¼ç¨æä½ç¬¦
     * EN: Dereference operator
     */
    T& operator*() { return *mapped_data; }
    const T& operator*() const { return *mapped_data; }

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
     * CN: ä¾¿å©æ¹æ³ï¼ä¿®æ¹æ°æ®å¹¶æ è®° dirty
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
     * CN: æäº¤ä¿®æ¹å?GPU
     *
     * è¡ä¸ºï¼?
     * 1. å¦æç¼å²åºæ StagedBuffer å³èï¼Unmap ä¼è§¦å?MarkDirty
     * 2. éæ° Mapï¼ç¡®ä¿?StagedBuffer çåæ´è¢«æ è®°
     * 3. å¯¹äº ReBAR/CPUOnly ç¼å²åºï¼Unmap/Map å¯è½æ¯æ æä½
     * 4. å¯¹äºéè¦æ¾å¼?flush çæåµï¼è°ç¨ buffer->Flush()
     *
     * EN: Commit changes to GPU
     *
     * Behavior:
     * 1. If buffer has associated StagedBuffer, Unmap triggers MarkDirty
     * 2. Re-Map to ensure StagedBuffer changes are tracked
     * 3. For ReBAR/CPUOnly buffers, Unmap/Map may be no-ops
     * 4. For cases needing explicit flush, call buffer->Flush()
     *
     * \return æ¯å¦æ§è¡äºæäº?/ Whether committed
     */
    bool Commit()
    {
        if(!dirty || !buffer || !mapped_data)
            return false;

        // æ¾å¼ Flush å½åæ°æ®å?GPU
        // Explicitly flush current data to GPU
        buffer->Write(mapped_data, sizeof(T));

        // å¯¹äº StagedBufferï¼éè¦æ è®°ä¸º dirty å¹¶æ£æ¥æäº?
        // For StagedBuffer, mark as dirty to trigger staged buffer submission
        buffer->Flush(aligned_size);

        dirty = false;
        return true;
    }

    /**
     * CN: ç«å³ Update çä¾¿å©æ¹æ³ï¼åæ§ UBOInstance::Update() å¼å®¹ï¼?
     * å¯¹åºæ§ç DeviceBufferMap::Update() è¡ä¸º
     * EN: Convenience method for immediate update (compatible with old UBOInstance::Update())
     * Maps to old DeviceBufferMap::Update() behavior
     */
    void ImmediateUpdate() const
    {
        if(!mapped_data || !buffer)
            return;

        buffer->Write(mapped_data, sizeof(T));
        buffer->Flush(aligned_size);

        // Reset frame counter after successful commit
        const_cast<DeviceBuffer*>(buffer)->ResetFramesSinceUpdate();
    }

    /**
     * CN: ä»£ç DeviceBuffer::Write æ¹æ³ - ç¨äºé¨åæ°æ®æ´æ°
     * EN: Proxy DeviceBuffer::Write method - for partial data update
     * ç¨äºæ´æ°ç»æä½ä¸­çæä¸ªå­æ®µèä¸æ¯æ´ä¸ªç»æä½
     */
    bool Write(const void *ptr, uint32_t offset, uint32_t size)
    {
        if(!buffer)
            return false;
        return buffer->Write(ptr, offset, size);
    }

    void Flush(uint32_t)
    {
        if(buffer)
            buffer->Flush(aligned_size);
    }

    /**
     * CN: ä»£ç DeviceBuffer::Flush æ¹æ³
     * EN: Proxy DeviceBuffer::Flush method
     */
    void Flush()
    {
        if(buffer)
            buffer->Flush(aligned_size);
    }

    // ===== UBOInstance å¼å®¹æ¥å£ / UBOInstance Compatible Interface =====

    /**
     * CN: è·åæè¿°ç¬¦éåç±»åï¼UBOInstance å¼å®¹ï¼?
     * EN: Get descriptor set type (UBOInstance compatible)
     */
    const DescriptorSetType& set_type() const { return desc_set_type; }

    /**
     * CN: è·å UBO åç§°ï¼UBOInstance å¼å®¹ï¼?
     * EN: Get UBO name (UBOInstance compatible)
     */
    const AnsiString& name() const { return ubo_name; }

    /**
     * CN: è·ååºå± DeviceBufferï¼UBOInstance å¼å®¹ï¼?
     * EN: Get underlying DeviceBuffer (UBOInstance compatible)
     */
    DeviceBuffer* ubo() const { return buffer; }

    /**
     * CN: Update() æ¹æ³ï¼UBOInstance å¼å®¹ï¼? å°è¯èªå¨ Flushï¼å¦ææ¯ StagedBufferï¼?
     * EN: Update() method (UBOInstance compatible) - auto-flush if needed
     */
    void Update() const override
    {
        ImmediateUpdate();
    }

    /**
     * CN: è·åç»æä½å¤§å°ï¼ç¼è¯æ¶å¸¸éï¼
     * EN: Get struct size (compile-time constant)
     */
    static constexpr VkDeviceSize GetSize()
    {
        return sizeof(T);
    }
};

VK_NAMESPACE_END
