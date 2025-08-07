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
         * @param chars_sets 要注册的字符合集
         */
        bool TileFont::Registry(TileUVFloatMap &uv_map,SortedSet<u32char> &chars_sets)
        {
            RefPoolStats stats;

            chars_sets.Delete(not_bitmap_chars);                                     //清除所有没有位图的字符

            to_res.Stats(stats,chars_sets.GetData(),chars_sets.GetCount());

            if(stats.not_found>stats.can_free+tile_data->GetFreeCount())         //不存在的字符数量总量>剩余可释放的闲置项+剩余可用的空余tile
                return(false);

            uv_map.Clear();

            RectScope2f null_uv_float;
            TileObject *to=nullptr;
            FontBitmap *bmp;

            if(stats.not_found>0)
            {
                tile_data->BeginCommit();
                for(const u32char cp:chars_sets)
                {
                    if(!not_bitmap_chars.Contains(cp))
                    if(!to_res.Get(cp,to))
                    {
                        bmp=source->GetCharBitmap(cp);

                        if(bmp)
                        {
                            to=tile_data->Commit(   bmp->data,
                                                    bmp->metrics_info.w*bmp->metrics_info.h,
                                                    bmp->metrics_info.w,bmp->metrics_info.h);

                            to_res.Add(cp,to);
                        }
                        else
                        {
                            not_bitmap_chars.Add(cp);
                            continue;
                        }
                    }

                    if(to)
                    {
                        uv_map.Add(cp,to->uv_float);
                    }
                    else
                    {
                        uv_map.Add(cp,null_uv_float);
                    }

                    to=nullptr;
                }
                tile_data->EndCommit();
            }
            else
            {
                for(const u32char cp:chars_sets)
                {
                    to_res.Get(cp,to);

                    uv_map.Add(cp,to->uv_float);
                }
            }

            return(true);
        }

        /**
         * 注销要使用的字符
         * @param ch_list 要注销的字符列表
         */
        void TileFont::Unregistry(const DataArray<u32char> &ch_list)
        {
            for(const u32char &ch:ch_list)
                to_res.Release(ch);
        }
    }//namespace graph
}//namespace hgl
