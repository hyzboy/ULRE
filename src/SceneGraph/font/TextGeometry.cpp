#include<hgl/graph/font/TextGeometry.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKShaderProgram.h>
#include<hgl/vk/VKVertexAttribBuffer.h>
#include<hgl/graph/geo/VKGeometryData.h>
#include<hgl/graph/geo/GeometryCreater.h>   // FloatToHalf（UV RG16F 写入）

namespace hgl::graph
{
    GeometryVertexFormat CreateTextGeometryVertexFormat()
    {
        GeometryVertexFormat gvf;

        gvf.Add(VertexSemantic::Position, VK_FORMAT_R16G16_SINT, 2, sizeof(int16) * 2);
        gvf.Add(VertexSemantic::TexCoord, VK_FORMAT_R16G16_SFLOAT, 2, sizeof(uint16) * 2);   // UV RG16F（half×2）

        return gvf;
    }

    TextGeometry::TextGeometry(VulkanDevice *dev,const GeometryVertexFormat &gvf,const uint32_t mc):Geometry("TextGeometry",nullptr)
    {
        device=dev;
        geometry_vertex_format=gvf;

        max_count=0;
        draw_char_count=0;

        vab_position=nullptr;
        vab_tex_coord=nullptr;
    }

    void TextGeometry::SetCharCount(const uint cc)
    {
        if (cc<=max_count)return;

        if(geometry_data)
        {
            if(geometry_data->GetVertexCount()<cc)
            {
                delete geometry_data;
                geometry_data=nullptr;
            }
        }

        max_count=power_to_2(cc);
        draw_char_count=cc;

        geometry_data=CreateGeometryData(device,geometry_vertex_format,max_count);

        geometry_data->CreateAllVAB();

        vab_position    =geometry_data->GetVAB(VAN::Position);
        vab_tex_coord   =geometry_data->GetVAB(VAN::TexCoord);
    }

    bool TextGeometry::WriteVertex    (const int16 *fp){if(!fp)return(false);if(!vab_position )return(false);return vab_position  ->Write(fp,draw_char_count);}

    bool TextGeometry::WriteTexCoord  (const float *fp)
    {
        if(!fp)return(false);
        if(!vab_tex_coord)return(false);

        // UV RG16F：float → half 位模式（VAB::Write 是原始字节拷贝——无格式转换）
        std::vector<uint16> half_data;
        half_data.resize(draw_char_count * 2);

        for(uint32_t i = 0; i < draw_char_count * 2; ++i)
            half_data[i] = FloatToHalf(fp[i]);

        return vab_tex_coord->Write(half_data.data(), draw_char_count);
    }
}//namespace hgl::graph
