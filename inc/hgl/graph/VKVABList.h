#pragma once

VK_NAMESPACE_BEGIN
class VABList
{
    uint32_t vab_count;
    VkBuffer *vab_list;
    VkDeviceSize *vab_offset;

    uint32_t write_count;

    friend class RenderCmdBuffer;

public:

    VABList(const uint32 bc)
    {
        vab_count=bc;
        vab_list=new VkBuffer[vab_count];
        vab_offset=new VkDeviceSize[vab_count];

        write_count=0;
    }

    ~VABList()
    {
        delete[] vab_offset;
        delete[] vab_list;
    }

    void Restart()
    {
        write_count=0;
    }

    const bool IsFull()const
    {
        return write_count>=vab_count;
    }

    void Add(const VkBuffer buf,const VkDeviceSize offset)
    {
        vab_list[write_count]=buf;
        vab_offset[write_count]=offset;

        ++write_count;
    }

    void Add(const VkBuffer *buf,const VkDeviceSize *offset,const uint32_t count)
    {
        hgl_cpy(vab_list  +write_count,buf,   count);

        if(offset)
        hgl_cpy(vab_offset+write_count,offset,count);
        else
        hgl_set<VkDeviceSize>(vab_offset+write_count,VkDeviceSize(0),count);

        write_count+=count;
    }
};//class VABList
VK_NAMESPACE_END
