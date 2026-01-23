#pragma once

#include<hgl/graph/VK.h>

VK_NAMESPACE_BEGIN
/**
* 原始图元数据缓冲区<Br>
* 提供在渲染之前的数据绑定信息
*/
struct GeometryDataBuffer
{
    uint32_t            vab_count;
    VkBuffer *          vab_list;

    // 理论上讲，每个VAB绑定时都是可以指定byte offsets的。但是随后Draw时，又可以指定vertexOffset。
    // 在我们支持的两种draw模式中，一种是每个模型一批VAB，所有VAB的offset都是0。
    // 另一种是使用VDM，为了批量渲染，所有的VAB又必须对齐，所以每个VAB单独指定offset也不可行。

    VkDeviceSize *      vab_offset;         //注意：这里的offset是字节偏移，不是顶点偏移

    IndexBuffer *       ibo;

    VertexDataManager * vdm;             //只是用来区分和比较的，不实际使用

public:

    GeometryDataBuffer(const uint32_t,IndexBuffer *,VertexDataManager *_v=nullptr);
    ~GeometryDataBuffer();

    std::strong_ordering operator<=>(const GeometryDataBuffer &geom_data_buffer)const
    {
        ptrdiff_t off;

        // Compare vdm pointers
        if(vdm && geom_data_buffer.vdm)
        {
            if(auto cmp = vdm <=> geom_data_buffer.vdm; cmp != 0)
                return cmp;
        }
        else if(vdm || geom_data_buffer.vdm)
        {
            return vdm ? std::strong_ordering::greater : std::strong_ordering::less;
        }

        // Compare vab_count
        if(auto cmp = vab_count <=> geom_data_buffer.vab_count; cmp != 0)
            return cmp;

        // Compare vab_list arrays
        if(auto cmp = mem_compare(vab_list, geom_data_buffer.vab_list, vab_count); cmp != 0)
            return cmp <=> 0;

        // Compare vab_offset arrays
        if(auto cmp = mem_compare(vab_offset, geom_data_buffer.vab_offset, vab_count); cmp != 0)
            return cmp <=> 0;

        // Compare ibo pointers
        return ibo <=> geom_data_buffer.ibo;
    }


    /**
     * 相等比较运算符
     * @param other 要比较的另一个 GeometryDataBuffer
     * @return 如果两个对象的所有成员都相等，返回 true；否则返回 false
     */
    bool operator==(const GeometryDataBuffer &other) const
    {
        // 比较 vdm 指针
        if(vdm != other.vdm)
            return false;
        
        // 比较 vab_count
        if(vab_count != other.vab_count)
            return false;
        
        // 比较 vab_list 数组
        if(mem_compare(vab_list, other.vab_list, vab_count) != 0)
            return false;
        
        // 比较 vab_offset 数组
        if(mem_compare(vab_offset, other.vab_offset, vab_count) != 0)
            return false;
        
        // 比较 ibo 指针
        if(ibo != other.ibo)
            return false;
        
        return true;
    }

    /**
     * 不相等比较运算符
     * @param other 要比较的另一个 GeometryDataBuffer
     * @return 如果两个对象的任何成员不相等，返回 true；否则返回 false
     */
    bool operator!=(const GeometryDataBuffer &other) const
    {
        return !(*this == other);
    }

    bool Update(const Geometry *,const VIL *);
};//struct GeometryDataBuffer
VK_NAMESPACE_END
