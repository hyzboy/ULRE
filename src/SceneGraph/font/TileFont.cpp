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

            int in_active_count;    //在活跃列表中的数量
            int in_idle_count;      //在闲置列表中的数量
            int out_count;          //不存在的字符数量
            int idle_left_count;    //闲置列表中可释放的数量

            to_res.Stats(ch_list,ch_count,&in_active_count,&in_idle_count,&out_count,&idle_left_count);

            if(out_count>idle_left_count+tile_data->GetFreeCount())         //不存在的字符数量总量>剩余可释放的闲置项+剩余可用的空余tile
                return(false);

            uv_map.ClearData();

            FontBitmap *bmp;
            cp=ch_list;

            if(out_count>0)
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
