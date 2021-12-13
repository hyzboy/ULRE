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
            PF_BC1_RGBUN,
            PF_BC1_RGBAUN,
            PF_BC2UN,
            PF_BC3UN,
            PF_BC4UN,
            PF_BC5UN,
            PF_BC6UF,
            PF_BC6SF,
            PF_BC7UN
        };

        constexpr uint32 CompressFormatBits[]={4,4,8,8,4,8,8,8,8};

        constexpr uint32 CompressFormatCount=sizeof(CompressFormatList)/sizeof(VkFormat);

        #pragma pack(push,1)
        struct TexPixelFormat
        {
            uint8 channels;                 //0: compress 1/2/3/4:normal

            union
            {
                struct
                {
                    char    colors[4];
                    uint8   bits[4];
                    uint8   datatype;
                };

                struct
                {
                    uint16 compress_format;
                };
            };

        public:

            const uint pixel_bits()const
            {
                return channels ?bits[0]+bits[1]+bits[2]+bits[3]
                                :CompressFormatBits[compress_format];
            }
        };//struct TexPixelFormat

        constexpr uint TexPixelFormatLength=sizeof(TexPixelFormat);

        struct alignas(8) TextureFileHeader
        {
            uint8 id_str[8];                ///<Texture\x1A
            uint8 version;                  ///<必须为0
            uint8 type;                     ///<贴图类型，等同于VkImageViewType

            union
            {
                uint32 length;              ///<长(1D纹理用)
                uint32 width;               ///<宽(2D/Cube纹理用)
            };

            uint32 height;                  ///<高(2D/3D/Cube纹理用)
              
            union
            {
                uint32 depth;               ///<深度(3D纹理用)
                uint32 layers;              ///<层数(Arrays纹理用)
            };

            TexPixelFormat pixel_format;    ///<象素格式
            uint8 mipmaps;                  ///<mipmaps
        };

        constexpr uint TextureFileHeaderLength=sizeof(TextureFileHeader);           //GPUBuffer内需要64位8字节对齐，如果此值不对齐，请想办法填0
        #pragma pack(pop)

        const uint32 ComputeMipmapBytes(uint32 length,uint32 bytes);
        const uint32 ComputeMipmapBytes(uint32 width,uint32 height,uint32 bytes);
        const uint32 ComputeMipmapBytes(uint32 width,uint32 height,uint32 depth,uint32 bytes);

        class TextureLoader
        {
        protected:

            VkImageViewType type;

            TextureFileHeader file_header;

            VkFormat format;

            uint32 mipmap_zero_total_bytes;                                     ///< 0 级mipmaps单个图象的总字节数
            uint32 total_bytes;                                                 ///<总字节数

        protected:

            virtual uint32 GetPixelsCount()const=0;
            virtual uint32 GetImageCount()const=0;                              ///<每个级别的图象数量
            virtual uint32 GetTotalBytes()const=0;                              ///<计算总字节数

        protected:

            virtual void *OnBegin(uint32)=0;
            virtual void  OnEnd()=0;
            virtual void  OnError(){}

        public:

            TextureLoader(const VkImageViewType &ivt){type=ivt;}

            virtual bool Load(io::InputStream *);
                    bool Load(const OSString &filename);
        };//class TextureLoader

        /**
         * 2D纹理加载器
         */
        class Texture2DLoader:public TextureLoader
        {
        protected:  // override functions

            uint32 GetPixelsCount()const override{return file_header.width*file_header.height;}
            uint32 GetImageCount()const override{return 1;}
            uint32 GetTotalBytes()const override
            {
                if(file_header.mipmaps<=1)
                    return mipmap_zero_total_bytes;
                else
                    return ComputeMipmapBytes(  file_header.width,
                                                file_header.height,
                                                mipmap_zero_total_bytes);
            }

        public:

            Texture2DLoader():TextureLoader(VK_IMAGE_VIEW_TYPE_2D){}
            virtual ~Texture2DLoader()=default;
        };//class Texture2DLoader
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_TEXTURE_LOADER_INCLUDE
