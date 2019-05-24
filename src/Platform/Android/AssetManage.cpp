#include<hgl/filesystem/AssetManage.h>
#include<hgl/io/InputStream.h>
#include<android/asset_manager.h>
#include<android/asset_manager_jni.h>

namespace hgl
{
    namespace
    {
        static AAssetManager *asset_manager=nullptr;

        class AndroidAssetInputStream:public io::InputStream
        {
            AAsset *asset;

        public:

            AndroidAssetInputStream(AAsset *a)
            {
                asset=a;
            }

            ~AndroidAssetInputStream()
            {
                Close();
            }

            void    Close() override
            {
                if(!asset)return;

                AAsset_close(asset);
                asset=nullptr;
            }

            int64   Read(void *data,int64 size) override
            {
                return AAsset_read(asset,data,size);
            }

            int64   Peek(void *data,int64 size) override
            {
                int64 left=Avaiable();

                if(left<size)
                    size=left;

                int64 result=Read(data,size);

                if(result>0)
                    Seek(-result,SeekOrigin::Current);

                return result;
            }

            int64   ReadFully(void *buf,int64 buf_size){return Read(buf,buf_size);}         ///<充分读取,保证读取到指定长度的数据(不计算超时)

            bool    CanRestart()const override{return(true);}
            bool    CanSeek()const override{return(true);}
            bool    CanSize()const override{return(true);}
            bool    CanPeek()const override{return(false);}

            bool    Restart() override
            {
                Seek(0,soBegin);
                return(true);
            }

            int64   Skip(int64 size) override
            {
                return Seek(size,SeekOrigin::Current);
            }

            int64   Seek(int64 offset,SeekOrigin so) override
            {
                return AAsset_seek64(asset,offset,(int)so);
            }

            int64   Tell()const override
            {
                return Seek(0,SeekOrigin::Current);
            }

            int64   GetSize()const override
            {
                return AAsset_getLength64(asset);
            }

            int64   Available()const override
            {
                return AAsset_getRemainingLength64(asset);
            }
        };//class AndroidAssetInputStream:public io::InputStream
    }//namespace

    void InitAssetManager(AAssetManager *am)
    {
        asset_manager=am;
    }

    io::InputStream *OpenAsset(const UTF8String &filename)
    {
        AAsset *asset = AAssetManager_open(asset_manager, filename.c_str(), AASSET_MODE_BUFFER);

        if(!asset)return(nullptr);

        return(new AndroidAssetInputStream(asset));
    }
}//namespace hgl
