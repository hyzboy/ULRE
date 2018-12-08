#ifndef HGL_GRAPH_TEXTURE_INCLUDE
#define HGL_GRAPH_TEXTURE_INCLUDE

#include<hgl/graph/TextureData.h>
#include<hgl/type/BaseString.h>
namespace hgl
{
    namespace graph
    {
        /**
        * 贴图类
        */
        class Texture
        {
        protected:

            uint type;                                                          ///<纹理类型(如GL_TEXTURE_2D类)

            uint texture_id;                                                    ///<纹理ID

            uint pixel_format;                                                  ///<象素格式(如RED,RG,RGB,RGBA,SRGB,SRGBA之类)
            uint video_format;                                                  ///<显存格式

        public:

            Texture(uint t,uint tid)
            {
                type=t;
                texture_id=tid;

                pixel_format=video_format=0;
            }

            virtual ~Texture()
            {
                glDeleteTextures(1,&texture_id);
            }

        public:

            uint    GetID           ()const{return texture_id;}                 ///<取得纹理ID
            uint    GetType         ()const{return type;}                       ///<取得类型
            uint    GetPixelFormat  ()const{return pixel_format;}               ///<取得象素格式
            uint    GetVideoFormat  ()const{return video_format;}               ///<取得显存中的数据格式

        public:

            virtual void GenMipmaps      ()=0;                                  ///<生成mipmaps
            virtual void GetMipmapLevel  (int &base_level,int &max_level)=0;    ///<取得贴图mipmaps级别
        };//class Texture
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_TEXTURE_INCLUDE
