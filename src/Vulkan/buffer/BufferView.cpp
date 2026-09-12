#include<hgl/vk/buffer/BufferView.h>

namespace hgl::graph{

BufferView::~BufferView()
{
    UnmapWindow();
}

void BufferView::SetBuffer(BufferOwner *buf)
{
    // 只有"窗口来自旧 buffer 的 Map"时才需要随换源解除（Unmap 会把已写范围标脏）。
    // 外部窗口（Arena/池行段）与 buffer 没有绑定关系——例如材质数据行池的顺序就是
    // 先挂外部窗口（cpu_base）再 SetBuffer(buf)（登记行缓冲），
    // 此时绝不能因为 SetBuffer 把窗口清掉。
    if(buffer != buf && !window_external)
        UnmapWindow();

    buffer  = buf;
    gpu_buf = buf ? buf->GetGPUBuffer() : nullptr;
}

bool BufferView::MapWindow(VkDeviceSize offset_bytes, VkDeviceSize size_bytes)
{
    if(window_ptr && !window_external
       && window_offset == offset_bytes && window_size == size_bytes)
        return true;            // 同一窗口已映射（幂等）

    UnmapWindow();

    if(!gpu_buf || size_bytes == 0)
        return false;

    void *ptr = gpu_buf->Map(offset_bytes, size_bytes);
    if(!ptr)
        return false;

    window_ptr      = ptr;
    window_offset   = offset_bytes;
    window_size     = size_bytes;
    window_external = false;

    return true;
}

void BufferView::AttachWindow(void *ptr, VkDeviceSize size_bytes)
{
    UnmapWindow();

    window_ptr      = ptr;
    window_offset   = 0;
    window_size     = size_bytes;
    window_external = true;
}

void BufferView::UnmapWindow()
{
    if(!window_ptr)
        return;

    if(gpu_buf && !window_external)
        gpu_buf->Unmap();       // StagedBuffer::Unmap 会把 mapped 范围标脏

    window_ptr      = nullptr;
    window_offset   = 0;
    window_size     = 0;
    window_external = false;
}

void BufferView::MarkWindowDirty()
{
    if(!gpu_buf || !window_ptr || window_external || window_size == 0)
        return;

    gpu_buf->MarkDirty(window_offset, window_size);
}

}//namespace hgl::graph
