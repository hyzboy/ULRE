#include<hgl/graph/font/TextPrimitive.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include"../Vulkan/VKPrimitiveData.h"

namespace hgl::graph
{
    TextPrimitive::TextPrimitive(VulkanDevice *dev,const VIL *_vil,const uint32_t mc):Primitive("TextPrimitive",nullptr)
    {
        device=dev;
        vil=_vil;

        max_count=0;
        draw_char_count=0;

        vab_position=nullptr;
        vab_tex_coord=nullptr;
    }

    void TextPrimitive::SetCharCount(const uint cc)
    {
        if (cc<=max_count)return;

        if(prim_data)
        {
            if(prim_data->GetVertexCount()<cc)
            {
                delete prim_data;
                prim_data=nullptr;
            }
        }

        max_count=power_to_2(cc);
        draw_char_count=cc;

        prim_data=CreatePrimitiveData(device,vil,max_count);

        prim_data->CreateAllVAB(max_count);

        vab_position    =prim_data->GetVAB(VAN::Position);
        vab_tex_coord   =prim_data->GetVAB(VAN::TexCoord);
    }

    bool TextPrimitive::WriteVertex    (const int16 *fp){if(!fp)return(false);if(!vab_position )return(false);return vab_position  ->Write(fp,draw_char_count);}
    bool TextPrimitive::WriteTexCoord  (const float *fp){if(!fp)return(false);if(!vab_tex_coord)return(false);return vab_tex_coord ->Write(fp,draw_char_count);}
}//namespace hgl::graph
