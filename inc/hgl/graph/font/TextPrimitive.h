#pragma once

#include<hgl/graph/VKPrimitive.h>
#include<hgl/type/SortedSet.h>
#include<hgl/graph/font/FontSource.h>

namespace hgl::graph
{
    /**
    * 文本图元
    */
    class TextPrimitive:public Primitive
    {
        VulkanDevice *  device;
        const VIL *     vil;

        uint        max_count=0;                                      ///<缓冲区最大容量
        uint        draw_char_count=0;

        VAB *       vab_position=nullptr;
        VAB *       vab_tex_coord=nullptr;

    protected:

        friend class TextLayout;
        friend class TextRender;

        U32CharSet chars_sets;

        const U32CharSet &GetCharsSets()const{return chars_sets;}
        void SetCharsSets(const U32CharSet &sl){chars_sets=sl;}
        void ClearCharsSets(){chars_sets.Clear();}

    public:

        TextPrimitive(VulkanDevice *dev,const VIL *_vil,const uint32_t mc);

        void SetCharCount   (const uint);

        bool WriteVertex    (const int16 *fp);
        bool WriteTexCoord  (const float *fp);

    public:

        const VkDeviceSize GetVertexCount()const override{return draw_char_count;}

        VAB *GetPositionVAB()const { return vab_position; }
        VAB *GetTexCoordVAB()const { return vab_tex_coord; }
    };//class TextPrimitive:public Primitive
}//namespace hgl::graph
