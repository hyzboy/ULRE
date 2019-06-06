#ifndef HGL_GRAPH_MATERIAL_INCLUDE
#define HGL_GRAPH_MATERIAL_INCLUDE

#include<hgl/graph/TextureType.h>
#include<hgl/type/Color4f.h>
#include<hgl/type/Set.h>
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
            UTF8String name;

            uint8 tex_count;

            MaterialTextureData *tex_list;

            Set<uint> uv_use;

            bool two_sided=false;
            uint shading_model=0;
            bool wireframe=false;

            uint blend_func;

            float opacity;              ///<透明度

            uint transparency_factor;

            float bump_scaling;
            float shininess;
            float reflectivity;         ///<反射率
            float shininess_strength;

            float refracti;             ///<折射率

            Color4f diffuse;
            Color4f ambient;
            Color4f specular;
            Color4f emission;
            Color4f transparent;        ///<透明色
            Color4f reflective;         ///<反射颜色

        public:

            MaterialData()
            {
                tex_count=0;
                tex_list=nullptr;
            }

            void InitDefaultColor()
            {
                diffuse.Set(1,1,1,1);
                specular.Set(0,0,0,1);
                ambient.Set(0.2f,0.2f,0.2f,1.0f);
                emission.Set(0,0,0,1);

                shininess=0;

                opacity=1.0f;
                refracti=0;
                transparent.Set(0,0,0,1);
                reflective.Set(1,1,1,1);
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