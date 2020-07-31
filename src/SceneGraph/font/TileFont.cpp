#include<hgl/graph/font/TileFont.h>
#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/graph/vulkan/VKFormat.h>

namespace hgl
{
    namespace graph
    {
        TileObject *TileObjectPool::Acquire()
        {
        }

        void TileObjectPool::Release(TileObject *)
        {
        }

        TileFont::TileFont(TileData *td,FontSource *fs)
        {
            tile_data=td;

            if(fs)
            {
                source=fs;
                source->RefAcquire(this);                
            }

            tile_pool=new TileObjectPool(tile_data);
            ch_tile_pool=new TileObjectManage(tile_pool);
        }

        TileFont::~TileFont()
        {
            delete ch_tile_pool;
            delete tile_pool;

            if(source)
                source->RefRelease(this);

            SAFE_CLEAR(tile_data);
        }
        
        /**
         * 注册要使用的字符
         * @param rs 每个字符在纹理中的UV坐标
         * @param ch_list 要注册的字符列表
         */
        bool TileFont::Registry(List<RectScope2f> &rs,const List<u32char> &ch_list)
        {
            
        }

        /**
         * 注销要使用的字符
         * @param ch_list 要注销的字符列表
         */
        void TileFont::Unregistry(const List<u32char> &ch_list)
        {
        }
    }//namespace graph
}//namespace hgl
