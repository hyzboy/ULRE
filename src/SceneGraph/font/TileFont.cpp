#include<hgl/graph/font/TileFont.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKFormat.h>

namespace hgl
{
    namespace graph
    {
        TileFont::TileFont(TileData *td,FontSource *fs)
        {
            tile_data=td;

            if(fs)
            {
                source=fs;
                source->RefAcquire(this);
            }
        }

        TileFont::~TileFont()
        {
            if(source)
                source->RefRelease(this);

            delete tile_data;
        }
        
        /**
         * 注册要使用的字符
         * @param rs 每个字符在纹理中的UV坐标
         * @param ch_list 要注册的字符列表
         */
        bool TileFont::Registry(TileUVFloatMap &uv_map,const u32char *ch_list,const int ch_count)
        {
            const u32char *cp=ch_list;
            TileObject *to;

            ResPoolStats stats;

            to_res.Stats(stats,ch_list,ch_count);

            if(stats.non_existent>stats.can_free+tile_data->GetFreeCount())         //不存在的字符数量总量>剩余可释放的闲置项+剩余可用的空余tile
                return(false);

            uv_map.ClearData();

            FontBitmap *bmp;
            cp=ch_list;

            if(stats.non_existent>0)
            {
                tile_data->BeginCommit();
                for(int i=0;i<ch_count;i++)
                {
                    if(!to_res.Get(*cp,to))
                    {
                        bmp=source->GetCharBitmap(*cp);

                        if(bmp)
                        {
                            to=tile_data->Commit(   bmp->data,
                                                    bmp->metrics_info.w*bmp->metrics_info.h,
                                                    bmp->metrics_info.w,bmp->metrics_info.h);

                            to_res.Add(*cp,to);
                        }
                        else
                        {
                            
                        }
                    }

                    uv_map.Add(*cp,to->uv_float);
                    ++cp;
                }
                tile_data->EndCommit();
            }
            else
            {
                for(int i=0;i<ch_count;i++)
                {
                    to_res.Get(*cp,to);

                    uv_map.Add(*cp,to->uv_float);
                    ++cp;
                }
            }

            return(true);
        }

        /**
         * 注销要使用的字符
         * @param ch_list 要注销的字符列表
         */
        void TileFont::Unregistry(const List<u32char> &ch_list)
        {
            const u32char *cp=ch_list.GetData();

            for(int i=0;i<ch_list.GetCount();i++)
            {
                to_res.Release(*cp);
                ++cp;
            }
        }
    }//namespace graph
}//namespace hgl
