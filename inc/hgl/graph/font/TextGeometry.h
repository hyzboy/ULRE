#pragma once

#include<hgl/graph/geo/VKGeometry.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>
#include<hgl/type/OrderedSet.h>
#include<hgl/graph/font/FontSource.h>

namespace hgl::ecs
{
    class TextRenderPipeline;
}//namespace hgl::ecs

namespace hgl::graph
{
    inline constexpr VkFormat TEXT_GEOMETRY_POSITION_FORMAT = VF_V2I16;
    inline constexpr VkFormat TEXT_GEOMETRY_TEXCOORD_FORMAT = VF_V2F;

    inline GeometryVertexFormat BuildTextGeometryVertexFormat()
    {
        GeometryVertexFormat gvf;
        gvf.Set(VAN::Position, TEXT_GEOMETRY_POSITION_FORMAT);
        gvf.Set(VAN::TexCoord, TEXT_GEOMETRY_TEXCOORD_FORMAT);
        return gvf;
    }

    namespace layout
    {
        class TextLayout;
    }//namespace layout

    /**
    * 文本图元
    */
    class TextGeometry:public Geometry
    {
        VulkanDevice *  device;
        GeometryVertexFormat geometry_vertex_format;

        uint        max_count=0;                                      ///<缓冲区最大容量
        uint        draw_char_count=0;

        VAB *       vab_position=nullptr;
        VAB *       vab_tex_coord=nullptr;

    protected:

        friend class layout::TextLayout;
        friend class TextRender;
        friend class ::hgl::ecs::TextRenderPipeline;

        U32CharSet chars_sets;

        const U32CharSet &GetCharsSets()const{return chars_sets;}
        void SetCharsSets(const U32CharSet &sl){chars_sets=sl;}
        void ClearCharsSets(){chars_sets.Clear();}

        ~TextGeometry() override {/*单纯用于避免被用户delete*/}

    public:

        TextGeometry(VulkanDevice *dev,const GeometryVertexFormat &gvf,const uint32_t mc);

        void SetCharCount   (const uint);

        bool WriteVertex    (const int16 *fp);
        bool WriteTexCoord  (const float *fp);

    public:

        const VkDeviceSize GetVertexCount()const override{return draw_char_count;}

        VAB *GetPositionVAB()const { return vab_position; }
        VAB *GetTexCoordVAB()const { return vab_tex_coord; }
    };//class TextGeometry:public Geometry
}//namespace hgl::graph
