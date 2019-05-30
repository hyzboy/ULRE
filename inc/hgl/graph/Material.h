#ifndef HGL_GRAPH_MATERIAL_INCLUDE
#define HGL_GRAPH_MATERIAL_INCLUDE

#include<hgl/graph/TextureType.h>
#include<hgl/type/Color4f.h>
namespace hgl
{
    namespace graph
    {
        struct MaterialTextureData
        {
            TextureType type=TextureType::None;

            int32 tex_id=-1;

            uint8 uvindex=0;
            float blend=0;
            uint8 op=0;
            uint8 wrap_mode[2]={0,0};
        };//struct MaterialTextureData

        struct MaterialData
        {
            uint8 tex_count;

            MaterialTextureData *tex_list;

            Set<int> uv_use;

            Color4f diffuse;
            Color4f specular;
            Color4f ambient;
            Color4f emission;

            float shininess=0;

            bool wireframe=false;
            bool two_sided=false;

        public:

            MaterialData()
            {
                tex_count=0;
                tex_list=nullptr;
            }

            void Init(const uint32 tc)
            {
                tex_count=tc;

                tex_list=new MaterialTextureData[tc];
            }

            ~MaterialData()
            {
                delete[] tex_list;
            }
        };//struct MaterialData
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_MATERIAL_INCLUDE