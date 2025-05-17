#include<hgl/graph/font/TextPrimitive.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKVertexAttribBuffer.h>

namespace hgl
{
    namespace graph
    {
        TextPrimitive::TextPrimitive(VulkanDevice *dev,Material *m,uint mc):Primitive(mc)
        {
            device=dev;
            mtl=m;

            max_count=0;

            vbo_position=nullptr;
            vbo_tex_coord=nullptr;
        }

        TextPrimitive::~TextPrimitive()
        {
            SAFE_CLEAR(vbo_tex_coord);
            SAFE_CLEAR(vbo_position);
        }

        void TextPrimitive::SetCharCount(const uint cc)
        {
            this->vertex_count=cc;
            if(cc<=max_count)return;

            max_count=power_to_2(cc);

            {
                if(vbo_position)
                    delete vbo_position;

                vbo_position    =device->CreateVBO(VF_V4I16,max_count);
                Set(VAN::Position,vbo_position);
            }

            {
                if(vbo_tex_coord)
                    delete vbo_tex_coord;

                vbo_tex_coord   =device->CreateVBO(VF_V4F,max_count);
                Set(VAN::TexCoord,vbo_tex_coord);
            }
        }

        bool TextPrimitive::WriteVertex    (const int16 *fp){if(!fp)return(false);if(!vbo_position )return(false);return vbo_position  ->Write(fp,vertex_count*4*sizeof(int16));}
        bool TextPrimitive::WriteTexCoord  (const float *fp){if(!fp)return(false);if(!vbo_tex_coord)return(false);return vbo_tex_coord ->Write(fp,vertex_count*4*sizeof(float));}
    }//namespace graph
}//namespace hgl
