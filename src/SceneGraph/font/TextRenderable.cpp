#include<hgl/graph/font/TextRenderable.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKMaterial.h>

namespace hgl
{
    namespace graph
    {
        TextRenderable::TextRenderable(vulkan::Device *dev,vulkan::Material *m,uint mc):vulkan::Renderable(mc)
        {
            device=dev;
            mtl=m;

            max_count=0;

            vab_position=nullptr;
            vab_tex_coord=nullptr;
        }

        TextRenderable::~TextRenderable()
        {
            SAFE_CLEAR(vab_tex_coord);
            SAFE_CLEAR(vab_position);
        }

        void TextRenderable::SetCharCount(const uint cc)
        {
            this->draw_count=cc;
            if(cc<=max_count)return;

            max_count=power_to_2(cc);

            {
                if(vab_position)
                    delete vab_position;

                vab_position      =device->CreateVAB(VAF_VEC4,max_count);
                Set(VAN::Position,vab_position);
            }

            {
                if(vab_tex_coord)
                    delete vab_tex_coord;

                vab_tex_coord   =device->CreateVAB(VAF_VEC4,max_count);
                Set(VAN::TexCoord,vab_tex_coord);
            }
        }

        bool TextRenderable::WriteVertex    (const float *fp){if(!fp)return(false);if(!vab_position   )return(false);return vab_position    ->Write(fp,draw_count*4*sizeof(float));}
        bool TextRenderable::WriteTexCoord  (const float *fp){if(!fp)return(false);if(!vab_tex_coord)return(false);return vab_tex_coord ->Write(fp,draw_count*4*sizeof(float));}
    }//namespace graph
}//namespace hgl
