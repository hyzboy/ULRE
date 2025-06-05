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

    bool Add(const VkBuffer buf,const VkDeviceSize offset)
    {
        if(IsFull())
        {
            //如果在这里出现错误，一般是材质的VertexInput与实现要使用的不匹配。很多时候是由于引擎自动添加的VertexInput但材质里没有。
            //比较典型的情况是创建材质时设置了不需要L2W,但实际又进行了传递
            return(false); //列表已满
        }

        vab_list[write_count]=buf;
        vab_offset[write_count]=offset;

        ++write_count;

        return(true);
    }

    bool Add(const VkBuffer *buf,const VkDeviceSize *offset,const uint32_t count)
    {
        if(!buf||!offset||!count)
            return(false);

        if(write_count+count>vab_count)
            return(false); //列表已满

        hgl_cpy(vab_list  +write_count,buf,   count);

        if(offset)
        hgl_cpy(vab_offset+write_count,offset,count);
        else
        hgl_set<VkDeviceSize>(vab_offset+write_count,VkDeviceSize(0),count);

        write_count+=count;
        return(true);
    }
};//class VABList
VK_NAMESPACE_END
