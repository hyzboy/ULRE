#include<hgl/graph/font/TileFont.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKFormat.h>

namespace hgl::graph
{
    TileFont::TileFont(TileData *td,FontSource *fs)
    {
        tile_data=td;
        source=fs;
    }

    TileFont::~TileFont()
    {
        delete tile_data;
        // source (FontSource) 不归 TileFont 所有，由外部创建者负责释放
    }

    /**
    * 注册要使用的字符
    * @param rs 每个字符在纹理中的UV坐标
    * @param chars_sets 要注册的字符合集
    */
    bool TileFont::Registry(TileUVFloatMap &uv_map,U32CharSet &chars_sets)
    {
        RefPoolStats stats;

        // 删除所有没有位图的字符
        for(const u32char cp : not_bitmap_chars)
        {
            chars_sets.Delete(cp);
        }

        // 构建临时数组用于Stats
        std::vector<u32char> temp_chars(chars_sets.begin(), chars_sets.end());
        to_res.Stats(stats, temp_chars.data(), temp_chars.size());

        const uint solid_reserve = solid_tile ? 0u : 1u;      //纯色 tile 常驻，需预留一格

        if(stats.not_found + solid_reserve > stats.can_free + tile_data->GetFreeCount())         //不存在的字符数量总量>剩余可释放的闲置项+剩余可用的空余tile
            return(false);

        uv_map.Clear();

        // 纯色 tile 只需在首次注册时生成一次，此后常驻图集。
        // 单独开一个提交窗口(与下方字形提交串行、不嵌套)，即便本批次全部字符已缓存(not_found==0)也能补齐。
        if(!solid_tile)
        {
            tile_data->BeginCommit();
            TileObject *st = tile_data->CommitSolid();
            tile_data->EndCommit();

            if(st)
            {
                solid_tile = st;

                // 采样 UV 退化为 tile 中心单点：双线性/三线性恒读内部 0xFF，杜绝相邻字形边缘渗色
                const float l = st->uv_float.GetLeft();
                const float t = st->uv_float.GetTop();
                const float cx = l + st->uv_float.GetWidth() * 0.5f;
                const float cy = t + st->uv_float.GetHeight() * 0.5f;

                solid_uv.Set(cx, cy, 0, 0);
            }
        }

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
    void TileFont::Unregistry(const std::vector<u32char> &ch_list)
    {
        for(const u32char &ch:ch_list)
            to_res.Release(ch);
    }
}//namespace hgl::graph
