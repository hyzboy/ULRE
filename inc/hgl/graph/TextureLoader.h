#ifndef HGL_GRAPH_TEXTURE_LOADER_INCLUDE
#define HGL_GRAPH_TEXTURE_LOADER_INCLUDE

#include<hgl/io/InputStream.h>
#include<hgl/type/String.h>
#include<hgl/graph/VKFormat.h>
#include<hgl/graph/BitmapData.h>
namespace hgl
{
    namespace graph
    {
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

            const uint pixel_bits()const;
        };//struct TexPixelFormat

        constexpr uint TexPixelFormatLength=sizeof(TexPixelFormat);

        struct alignas(8) TextureFileHeader
        {
            uint8 id_str[7];                ///<Texture\x1A
            uint8 version;                  ///<必须为0
            uint8 type;                     ///<贴图类型，等同于VkImageViewType

            union
            {
                uint32 length;              ///<长(1D纹理用)
                uint32 width;               ///<宽(2D/3D/Cube纹理用)
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

            TextureFileHeader file_header;

            VkFormat format;

            uint32 mipmap_zero_total_bytes;                                     ///< 0 级mipmaps单个图象的总字节数
            uint32 total_bytes;                                                 ///<总字节数

        protected:

            virtual uint32 GetPixelsCount()const=0;
            virtual uint32 GetTotalBytes()const=0;                              ///<计算总字节数

        protected:

            virtual void *OnBegin(uint32,const VkFormat &)=0;
            virtual bool  OnEnd()=0;
            virtual void  OnError(){}

        public:

            const TextureFileHeader &   GetFileHeader   ()const{return file_header;}
            const VkFormat &            GetTextureFormat()const{return format;}
            const uint32                GetZeroMipmapBytes()const{return mipmap_zero_total_bytes;}

        public:

            TextureLoader()
            {
                format=VK_FORMAT_UNDEFINED;
                mipmap_zero_total_bytes=0;
                total_bytes=0;
            }

            virtual ~TextureLoader()=default;

            virtual bool Load(io::InputStream *);
                    bool Load(const OSString &filename);
        };//class TextureLoader

        /**
         * 1D纹理加载器
         */
        class Texture1DLoader:public TextureLoader
        {
        protected:  // override functions

            uint32 GetPixelsCount()const override{return file_header.length;}
            uint32 GetTotalBytes()const override
            {
                if(file_header.mipmaps<=1)
                    return mipmap_zero_total_bytes;
                else
                    return ComputeMipmapBytes(  file_header.length,
                        mipmap_zero_total_bytes);
            }

        public:

            using TextureLoader::TextureLoader;
            virtual ~Texture1DLoader()=default;
        };//class Texture1DLoader

        /**
         * 2D纹理加载器
         */
        class Texture2DLoader:public TextureLoader
        {
        protected:  // override functions

            uint32 GetPixelsCount()const override{return file_header.width*file_header.height;}
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

            using TextureLoader::TextureLoader;
            virtual ~Texture2DLoader()=default;
        };//class Texture2DLoader

        /**
         * 3D纹理加载器
         */
        class Texture3DLoader:public TextureLoader
        {
        protected:  // override functions

            uint32 GetPixelsCount()const override{return file_header.width*file_header.height*file_header.depth;}
            uint32 GetTotalBytes()const override
            {
                if(file_header.mipmaps<=1)
                    return mipmap_zero_total_bytes;
                else
                    return ComputeMipmapBytes(  file_header.width,
                        file_header.height,
                        file_header.depth,
                        mipmap_zero_total_bytes);
            }

        public:

            using TextureLoader::TextureLoader;
            virtual ~Texture3DLoader()=default;
        };//class Texture3DLoader

        /**
         * Cube纹理加载器
         */
        class TextureCubeLoader:public TextureLoader
        {
        protected:  // override functions

            uint32 GetPixelsCount()const override{return file_header.width*file_header.height;}
            uint32 GetTotalBytes()const override
            {
                if(file_header.mipmaps<=1)
                    return mipmap_zero_total_bytes*6;
                else
                    return ComputeMipmapBytes(  file_header.width,
                        file_header.height,
                        mipmap_zero_total_bytes)*6;
            }

        public:

            using TextureLoader::TextureLoader;
            virtual ~TextureCubeLoader()=default;
        };//class TextureCubeLoader

        /**
         * 1D纹理阵列加载器
         */
        class Texture1DArrayLoader:public TextureLoader
        {
        protected:  // override functions

            uint32 GetPixelsCount()const override{return file_header.length*file_header.layers;}
            uint32 GetTotalBytes()const override
            {
                if(file_header.mipmaps<=1)
                    return mipmap_zero_total_bytes*file_header.layers;
                else
                    return ComputeMipmapBytes(  file_header.length,
                        mipmap_zero_total_bytes)*file_header.layers;
            }

        public:

            using TextureLoader::TextureLoader;
            virtual ~Texture1DArrayLoader()=default;
        };//class Texture1DArrayLoader

        /**
         * 2D纹理阵列加载器
         */
        class Texture2DArrayLoader:public TextureLoader
        {
        protected:  // override functions

            uint32 GetPixelsCount()const override{return file_header.width*file_header.height*file_header.layers;}
            uint32 GetTotalBytes()const override
            {
                if(file_header.mipmaps<=1)
                    return mipmap_zero_total_bytes*file_header.layers;
                else
                    return ComputeMipmapBytes(  file_header.width,
                        file_header.height,
                        mipmap_zero_total_bytes)*file_header.layers;
            }

        public:

            using TextureLoader::TextureLoader;
            virtual ~Texture2DArrayLoader()=default;
        };//class Texture2DArrayLoader

        /**
         * Cube纹理阵列加载器
         */
        class TextureCubeArrayLoader:public TextureLoader
        {
        protected:  // override functions

            uint32 GetPixelsCount()const override{return file_header.width*file_header.height;}
            uint32 GetTotalBytes()const override
            {
                if(file_header.mipmaps<=1)
                    return mipmap_zero_total_bytes*6*file_header.layers;
                else
                    return ComputeMipmapBytes(  file_header.width,
                        file_header.height,
                        mipmap_zero_total_bytes)*6*file_header.layers;
            }

        public:

            using TextureLoader::TextureLoader;
            virtual ~TextureCubeArrayLoader()=default;
        };//class TextureCubeArrayLoader
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_TEXTURE_LOADER_INCLUDE
