#pragma once

#include<hgl/vk/VKBufferOwner.h>
#include<hgl/graph/ShaderBufferSource.h>

namespace hgl::graph{

class VulkanDevice;

/**
 * BufferAccessBase (Layer 3) —— 视图基类：算地址 / 定范围 / 给类型化读写语法
 *
 * 视图【不拥有】数据源：buffer / gpu_buf 都是引用，生命周期归调用方、registry 或数据池。
 *
 * 唯一的 CPU 可写区域（“窗口”）记录在基类，由两处提供：
 *   1. L2 的 Map()（MapWindow()）——窗口 = (offset_bytes, size_bytes)，
 *      可以只是 buffer 的一段（如 VAB 的 element_offset 窗口）；
 *   2. 外部内存（AttachWindow()）——Arena/池行段直接给出 CPU 基址，不经 Map/Unmap。
 * 各视图不再各自保存 mapped_pointer，只在窗口上做自己的类型化语法。
 *
 * 脏标记只走 L2：MarkWindowDirty() → IGPUBuffer::MarkDirty(窗口范围)。
 * 视图【不再自持 dirty】（消除与 L2 的双记账）；提交/上传由
 * ECS RenderBufferUploadSystem 按 L2 的脏范围统一执行。
 */
class BufferAccessBase
{
protected:
    VkBufferOwner *buffer  = nullptr;  // descriptor / GetBuffer() / static_cast — 非拥有
    IGPUBuffer   *gpu_buf = nullptr;   // 写路径专用，SetBuffer() 时同步赋值

    // A6-2b-b2：PerObject 集已退场——默认归属改为 Scene（UBO 唯一现存集；
    // 该字段为历史描述符归属记录，BDA 后行表/UBO 均无绑定语义）。
    DescriptorSetType desc_set_type = DescriptorSetType::Scene;
    AnsiString ubo_name;

    // ---- 窗口：唯一记录的 CPU 可写区域 ----
    void *        window_ptr      = nullptr;    ///< 窗口基址（buffer 映射 或 外部内存）
    VkDeviceSize  window_offset   = 0;          ///< 窗口在 buffer 内的字节偏移
    VkDeviceSize  window_size     = 0;          ///< 窗口字节数
    bool          window_external = false;      ///< true = 指向外部内存：Map/Unmap/标脏均 no-op

protected:

    void SetBuffer(VkBufferOwner *buf);

    /** 映射窗口（幂等：同一窗口已映射则直接成功）。offset/size 单位：字节。 */
    bool MapWindow(VkDeviceSize offset_bytes, VkDeviceSize size_bytes);

    /** 挂接外部内存窗口（Arena/池行段）：不 Map、不标脏（宿主直写即生效）。 */
    void AttachWindow(void *ptr, VkDeviceSize size_bytes);

    /** 解除窗口（外部窗口只清记录）。 */
    void UnmapWindow();

    /** 把窗口范围标脏交 L2；无 gpu_buf 或外部窗口时 no-op。 */
    void MarkWindowDirty();

    void SetUBOMeta(const DescriptorSetType &dst, const AnsiString &name)
    {
        desc_set_type = dst;
        ubo_name = name;
    }

    void MoveFrom(BufferAccessBase &&other)
    {
        buffer        = other.buffer;
        gpu_buf       = other.gpu_buf;
        desc_set_type = other.desc_set_type;
        ubo_name      = other.ubo_name;

        window_ptr      = other.window_ptr;
        window_offset   = other.window_offset;
        window_size     = other.window_size;
        window_external = other.window_external;

        other.buffer    = nullptr;
        other.gpu_buf   = nullptr;
        other.window_ptr      = nullptr;
        other.window_offset   = 0;
        other.window_size     = 0;
        other.window_external = false;
    }

public:
    BufferAccessBase() = default;
    virtual ~BufferAccessBase();

    BufferAccessBase(const BufferAccessBase &) = delete;
    BufferAccessBase &operator=(const BufferAccessBase &) = delete;

    VkBufferOwner *GetBuffer()             { return buffer; }
    const VkBufferOwner *GetBuffer() const { return buffer; }

    /**
     * Returns the cached IGPUBuffer* for CPU writes.
     * Populated by SetBuffer(); nullptr for pure device-local buffers (no upload path).
     */
    IGPUBuffer       *GetGPUBuffer()       { return gpu_buf; }
    const IGPUBuffer *GetGPUBuffer() const { return gpu_buf; }

    // ===== 窗口访问：各视图用它实现自己的数据语法 =====
    void *        GetWindowData()   const { return window_ptr; }
    VkDeviceSize  GetWindowOffset() const { return window_offset; }
    VkDeviceSize  GetWindowSize()   const { return window_size; }
    bool          HasWindow()       const { return window_ptr != nullptr; }

    bool Write(const void *ptr, uint32_t offset, uint32_t size)
    {
        if(!gpu_buf) return false;
        return gpu_buf->Write(ptr, (VkDeviceSize)offset, (VkDeviceSize)size);
    }

    void Flush(uint32_t size)
    {
        if(gpu_buf)
            gpu_buf->MarkDirty(0, static_cast<VkDeviceSize>(size));
    }

    // Optional update hook for structured accessors.
    virtual void Update() const {}

    // ===== UBO metadata access =====
    const DescriptorSetType &set_type() const { return desc_set_type; }
    const AnsiString &name()            const { return ubo_name; }
    IGPUBuffer *ubo()                   const { return gpu_buf; }
};//class BufferAccessBase

}//namespace hgl::graph
