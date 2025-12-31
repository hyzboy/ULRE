#include<hgl/graph/Bitmap2DLoader.h>

namespace hgl::graph
{
    Bitmap2DLoader::~Bitmap2DLoader()
    {
        SAFE_CLEAR(bmp);
    }

    void *Bitmap2DLoader::OnBegin(uint32 total_bytes,const VkFormat &)
    {
        SAFE_CLEAR(bmp);

        bmp=new BitmapData;

        bmp->width      =file_header.width;
        bmp->height     =file_header.height;
        bmp->total_bytes=total_bytes;

        bmp->data=new char[total_bytes];

        return bmp->data;
    }

    BitmapData *Bitmap2DLoader::GetBitmap()
    {
        BitmapData *result=bmp;
        bmp=nullptr;
        return result;
    }

    BitmapData *LoadBitmapFromFile(const OSString &filename)
    {
        Bitmap2DLoader loader;

        if(!loader.Load(filename))
            return(nullptr);

        return loader.GetBitmap();
    }
}//namespace hgl::graph
