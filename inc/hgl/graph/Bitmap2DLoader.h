#ifndef HGL_GRAPH_BITMAP2D_LOADER_INCLUDE
#define HGL_GRAPH_BITMAP2D_LOADER_INCLUDE

#include<hgl/graph/TextureLoader.h>
namespace hgl
{
    namespace graph
    {
        /**
        * 2DŒªÕºº”‘ÿ
        */
        class Bitmap2DLoader:public Texture2DLoader
        {
        protected:

            BitmapData *bmp=nullptr;

        public:

            using Texture2DLoader::Texture2DLoader;
            ~Bitmap2DLoader();

            void *OnBegin(uint32 total_bytes) override;
            void OnEnd() override {}

            BitmapData *GetBitmap();
        };//class Bitmap2DLoader

        BitmapData *LoadBitmapFromFile(const OSString &filename);
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_BITMAP2D_LOADER_INCLUDE
