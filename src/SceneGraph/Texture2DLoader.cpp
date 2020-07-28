#include<hgl/graph/TextureLoader.h>
#include<hgl/graph/Bitmap.h>
#include<hgl/io/FileInputStream.h>
#include<hgl/log/LogInfo.h>

namespace hgl
{
    namespace graph
    {
        bool Texture2DLoader::Load(io::InputStream *is)
        {
            if(!is)return(false);

            if(is->Read(&file_header,sizeof(Tex2DFileHeader))!=sizeof(Tex2DFileHeader))
                return(false);

            if(file_header.version!=2)
                return(false);

            if(file_header.total_bytes()==0)
                return(false);

            const uint total_bytes=file_header.total_bytes();

            if(is->Available()<total_bytes)
                return(false);

            void *ptr=OnBegin(total_bytes);
            if(!ptr)
                return(false);
            
            if(is->Read(ptr,total_bytes)!=total_bytes)
                OnError();
            
            OnEnd();

            return(true);
        }

        bool Texture2DLoader::Load(const OSString &filename)
        {
            io::OpenFileInputStream fis(filename);

            if(!fis)
            {
                LOG_ERROR(OS_TEXT("[ERROR] open texture file<")+filename+OS_TEXT("> failed."));
                return(false);
            }

            return this->Load(&fis);
        }
    }//namespace graph

    namespace graph
    {
        class Bitmap2DLoader:public Texture2DLoader
        {
        protected:

            BitmapData *bmp=nullptr;

        public:

            ~Bitmap2DLoader()
            {
                SAFE_CLEAR(bmp);
            }

            void *OnBegin(uint32 total_bytes) override
            {
                bmp=new BitmapData;

                bmp->width      =file_header.width;
                bmp->height     =file_header.height;
                bmp->total_bytes=total_bytes;

                bmp->data=new char[total_bytes];

                return bmp->data;
            }

            void OnEnd() override {}

            BitmapData *GetBitmap()
            {
                BitmapData *result=bmp;
                bmp=nullptr;
                return result;
            }
        };//class Bitmap2DLoader
    
        BitmapData *LoadBitmapFromFile(const OSString &filename)
        {
            Bitmap2DLoader loader;

            if(!loader.Load(filename))
                return(false);

            return loader.GetBitmap();
        }
    }//namespace graph
}//namespace hgl
