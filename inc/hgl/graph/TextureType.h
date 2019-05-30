#ifndef HGL_GRAPH_TEXTURE_TYPE
#define HGL_GRAPH_TEXTURE_TYPE
namespace hgl
{
    namespace graph
    {
        /**
         * 纹理类型
         */
        enum class TextureType
        {
            None=0,

            Diffuse,
            Specular,
            Ambient,
            Emissive,
            Height,
            Normals,
            Shininess,
            Opacity,
            Displacement,
            Lightmap,
            Reflection,
            AO,

            Albedo,
            Metallic,
            Rougness,

            BEGIN_REANGE=Diffuse,
            END_RANGE=Rougness,
            RANGE_SIZE=END_RANGE-BEGIN_REANGE+1
        };//enum class TextureType
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_TEXTURE_TYPE
