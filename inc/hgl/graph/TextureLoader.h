#ifndef HGL_GRAPH_TEXTURE_LOADER_INCLUDE
#define HGL_GRAPH_TEXTURE_LOADER_INCLUDE

#include<hgl/io/InputStream.h>
#include<hgl/type/String.h>
#include<hgl/graph/Bitmap.h>
#include<hgl/graph/VKFormat.h>
namespace hgl
{
    namespace graph
    {
        constexpr VkFormat CompressFormatList[]=
        {
            FMT_BC1_RGBUN,
            FMT_BC1_RGBAUN,
            FMT_BC2UN,
            FMT_BC3UN,
            FMT_BC4UN,
            FMT_BC5UN,
            FMT_BC6UF,
            FMT_BC6SF,
            FMT_BC7UN
        };

        constexpr uint32 CompressFormatBits[]={4,4,8,8,4,8,8,8,8};

        constexpr uint32 CompressFormatCount=sizeof(CompressFormatList)/sizeof(VkFormat);

        #pragma pack(push,1)
        struct Tex2DFileHeader
        {
            uint8   id[6];                  ///<Tex2D\x1A
            uint8   version;                ///<必须为2
            uint8   mipmaps;
            uint32  width;
            uint32  height;
            uint8   channels;

        public:

            const uint pixel_count()const{return width*height;}
        };//struct Tex2DFileHeader

        struct TexPixelFormat
        {
            char    colors[4];
            uint8   bits[4];
            uint8   datatype;

        public:

            const uint pixel_bits()const{return bits[0]+bits[1]+bits[2]+bits[3];}
            const uint pixel_bytes()const{return pixel_bits()>>3;}
        };//struct TexPixelFormat
        #pragma pack(pop)

        /**
         * 2D纹理加载器
         */
        class Texture2DLoader
        {
        protected:

            Tex2DFileHeader file_header;

            union
            {
                TexPixelFormat  pixel_format;
                uint16          compress_format;
            };

        protected:

            uint32 mipmap_zero_total_bytes;

            uint32 ComputeTotalBytes();
            
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
        
        /**
         * 2D位图加载
         */
        class Bitmap2DLoader:public Texture2DLoader
        {
        protected:

            BitmapData *bmp=nullptr;

        public:

            ~Bitmap2DLoader();

            void *OnBegin(uint32 total_bytes) override;
            void OnEnd() override {}

            BitmapData *GetBitmap();
        };//class Bitmap2DLoader
    
        BitmapData *LoadBitmapFromFile(const OSString &filename);
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_TEXTURE_LOADER_INCLUDE
