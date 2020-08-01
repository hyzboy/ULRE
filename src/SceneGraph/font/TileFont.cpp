#include<hgl/graph/font/TileFont.h>
#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/graph/vulkan/VKFormat.h>

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
        bool TileFont::Registry(List<TileUVFloat> &rs,const List<u32char> &ch_list)
        {
            const u32char *cp=ch_list.GetData();
            TileObject *to;

            List<u32char> new_chars;

            for(uint i=0;i<ch_list.GetCount();i++)
            {
                if(!to_res.KeyExist(*cp))
                    new_chars.Add(*cp);

                cp++;
            }

            if(new_chars.GetCount()>tile_data->GetFreeCount())      //剩余空间不够了
                return(false);

            rs.ClearData();
            rs.SetCount(ch_list.GetCount());

            FontBitmap *bmp;
            TileUVFloat *tp=rs.GetData();
            cp=ch_list.GetData();

            if(new_chars.GetCount())
            {
                tile_data->BeginCommit();
                for(uint i=0;i<ch_list.GetCount();i++)
                {
                    if(!to_res.Get(*cp,to))
                    {
                        bmp=source->GetCharBitmap(*cp);
                        to=tile_data->Commit(   bmp->data,
                                                bmp->metrics_info.w*bmp->metrics_info.h,
                                                bmp->metrics_info.w,bmp->metrics_info.h);

                        to_res.Add(*cp,to);
                    }

                    ++cp;

                    *tp=to->uv_float;
                    ++tp;
                }
                tile_data->EndCommit();
            }
            else
            {
                for(uint i=0;i<ch_list.GetCount();i++)
                {
                    to_res.Get(*cp,to);
                    ++cp;

                    *tp=to->uv_float;
                    ++tp;
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

            for(uint i=0;i<ch_list.GetCount();i++)
            {
                to_res.Release(*cp);
                ++cp;
            }
        }
    }//namespace graph
}//namespace hgl
