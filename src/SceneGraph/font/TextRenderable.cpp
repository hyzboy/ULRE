#include<hgl/graph/font/TextRenderable.h>

namespace hgl
{
    namespace graph
    {
        namespace
        {
            vulkan::Pipeline *text_pipeline=nullptr;
        }//namespace

        TextRenderable::TextRenderable(Device *dev,int mc)
        {
            device=dev;

            render_obj=nullptr;
            vab_vertex=nullptr;
            vab_tex_coord=nullptr;

            max_count=mc;
            cur_count=mc;

            SetMaxCount(mc);
        }

        TextRenderable::~TextRenderable()
        {
            SAFE_CLEAR(vab_tex_coord);
            SAFE_CLEAR(vab_vertex);
            SAFE_CLEAR(render_obj);
        }

        void TextRenderable::SetMaxCount(int mc)
        {
            if(mc<max_count)return;

            max_count=mc;

            if(vab_vertex)
            {
                delete vab_vertex;

                vab_vertex      =device->CreateVAB(VAF_VEC4,max_count);

                render_obj->Set(VAN::Vertex,vab_vertex);
            }

            if(vab_tex_coord)
            {
                delete vab_tex_coord;

                vab_tex_coord   =device->CreateVAB(VAF_VEC4,max_count);

                render_obj->Set(VAN::TexCoord,vab_tex_coord);
            }
        }
    }//namespace graph
}//namespace hgl
