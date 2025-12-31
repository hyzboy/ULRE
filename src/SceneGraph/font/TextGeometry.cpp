#include<hgl/graph/font/TextGeometry.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include"../Vulkan/VKGeometryData.h"

namespace hgl::graph
{
    TextGeometry::TextGeometry(VulkanDevice *dev,const VIL *_vil,const uint32_t mc):Geometry("TextGeometry",nullptr)
    {
        device=dev;
        vil=_vil;

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

        geometry_data=CreateGeometryData(device,vil,max_count);

        geometry_data->CreateAllVAB();

        vab_position    =geometry_data->GetVAB(VAN::Position);
        vab_tex_coord   =geometry_data->GetVAB(VAN::TexCoord);
    }

    bool TextGeometry::WriteVertex    (const int16 *fp){if(!fp)return(false);if(!vab_position )return(false);return vab_position  ->Write(fp,draw_char_count);}
    bool TextGeometry::WriteTexCoord  (const float *fp){if(!fp)return(false);if(!vab_tex_coord)return(false);return vab_tex_coord ->Write(fp,draw_char_count);}
}//namespace hgl::graph
