#pragma once

VK_NAMESPACE_BEGIN
class VBOList
{
    uint32_t binding_count;
    VkBuffer *buffer_list;
    VkDeviceSize *buffer_offset;

    uint32_t write_count;

    friend class RenderCmdBuffer;

public:

    VBOList(const uint32 bc)
    {
        binding_count=bc;
        buffer_list=new VkBuffer[binding_count];
        buffer_offset=new VkDeviceSize[binding_count];

        write_count=0;
    }

    ~VBOList()
    {
        delete[] buffer_offset;
        delete[] buffer_list;
    }

    void Restart()
    {
        write_count=0;
    }

    const bool IsFull()const
    {
        return write_count>=binding_count;
    }

    void Add(const VkBuffer buf,const VkDeviceSize offset)
    {
        buffer_list[write_count]=buf;
        buffer_offset[write_count]=offset;

        ++write_count;
    }

    void Add(const VkBuffer *buf,const VkDeviceSize *offset,const uint32_t count)
    {
        hgl_cpy(buffer_list  +write_count,buf,   count);
        hgl_cpy(buffer_offset+write_count,offset,count);

        write_count+=count;
    }
};//class VBOList
VK_NAMESPACE_END
