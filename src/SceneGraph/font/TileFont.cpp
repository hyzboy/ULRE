#include<hgl/graph/font/TileFont.h>
#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/graph/vulkan/VKFormat.h>

namespace hgl
{
    namespace graph
    {
        namespace
        {
            using TileObjectPointer=TileObject *;

            class TileObjectPool:public DataPool<TileObjectPointer>
            {
                TileData *tile_data;

            public:

                TileObjectPool(TileData *td):tile_data(td){}

                bool Acquire(TileObjectPointer &top) override
                {
                    top=tile_data->Acquire();
                
                    return(top);
                }

                void Release(TileObjectPointer) override 
                {
                }
            };//class TileObjectPool:public DataPool<TileObjectPointer>
        }//namespace

        TileFont::TileFont(TileData *td,FontSource *fs)
        {
            tile_data=td;

            if(fs)
            {
                source=fs;
                source->RefAcquire(this);
            }

            to_pool=new TileObjectPool(tile_data);
            to_res=new TileResPool(to_pool);
        }

        TileFont::~TileFont()
        {
            delete to_res;
            delete to_pool;

            if(source)
                source->RefRelease(this);

            delete tile_data;
        }
        
        /**
         * 注册要使用的字符
         * @param rs 每个字符在纹理中的UV坐标
         * @param ch_list 要注册的字符列表
         */
        bool TileFont::Registry(List<TileUVFloat> &rs,const List<u32char> &ch_list)
        {
            const u32char *cp=ch_list.GetData();
            TileObject *to;

            List<u32char> new_chars;

            for(uint i=0;i<ch_list.GetCount();i++)
            {
                if(!to_res->Get(*cp,to))
                    new_chars.Add(*cp);

                cp++;
            }

            tile_data->BeginCommit();
            tile_data->EndCommit();
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
