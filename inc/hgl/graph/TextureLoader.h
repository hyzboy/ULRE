#ifndef HGL_GRAPH_TEXTURE_LOADER_INCLUDE
#define HGL_GRAPH_TEXTURE_LOADER_INCLUDE

#include<hgl/io/InputStream.h>
#include<hgl/type/BaseString.h>
namespace hgl
{
    namespace graph
    {
        #pragma pack(push,1)
        struct Tex2DFileHeader
        {
            uint8   id[6];                  ///<Tex2D\x1A
            uint8   version;                ///<必须为2
            bool    mipmaps;
            uint32  width;
            uint32  height;
            uint8   channels;
            char    colors[4];
            uint8   bits[4];
            uint8   datatype;

        public:

            const uint pixel_count()const{return width*height;}
            const uint pixel_bytes()const{return (bits[0]+bits[1]+bits[2]+bits[3])>>3;}
            const uint total_bytes()const{return pixel_count()*pixel_bytes();}
        };//
        #pragma pack(pop)

        /**
         * 2D纹理加载器
         */
        class Texture2DLoader
        {
        protected:

            Tex2DFileHeader file_header;

        protected:        

            virtual void *OnBegin(uint32)=0;
            virtual void  OnEnd()=0;
            virtual void  OnError(){}

        public:

            const Tex2DFileHeader *GetFileHeader()const{return &file_header;}

        public:

            virtual ~Texture2DLoader()=default;

            bool Load(io::InputStream *is);
            bool Load(const OSString &filename);
        };//class Texture2DLoader
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_TEXTURE_LOADER_INCLUDE
