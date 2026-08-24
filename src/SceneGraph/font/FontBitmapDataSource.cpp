#include<hgl/graph/font/FontSource.h>
#include<hgl/util/sdf.h>
#include<cstring>

namespace hgl::graph
{
    FontBitmap *FontBitmapDataSource::GetCharBitmap(const u32char &ch)
    {
        if(hgl::is_space(ch))return(nullptr);   //不能显示的数据或是空格

        FontBitmap *bmp;

        if(chars_bitmap.Get(ch,bmp))
            return bmp;

        bmp=new FontBitmap;

        memset(bmp,0,sizeof(FontBitmap));

        if(!MakeCharBitmap(bmp,ch))
        {
            delete bmp;
            chars_bitmap.Add(ch,nullptr);
            return(nullptr);
        }
        else
        {
            if(sdf_enabled)
            {
                // SDF 转换：将灰度位图转为距离场
                int32_t w = bmp->metrics_info.w;
                int32_t h = bmp->metrics_info.h;

                if(w > 0 && h > 0)
                {
                    int32_t spread = 8; // 默认 spread，后续可配置化
                    int32_t temp_size = sdf_temp_buffer_size(w, h);

                    uint8_t *sdf_dst = new uint8_t[w * h];
                    void *temp = new uint8_t[temp_size];

                    sdf_generate(w, h, spread, bmp->data, sdf_dst, temp);

                    memcpy(bmp->data, sdf_dst, w * h);

                    delete[] sdf_dst;
                    delete[] static_cast<uint8_t*>(temp);
                }
            }

            chars_bitmap.Add(ch,bmp);
            return bmp;
        }
    }

    const bool FontBitmapDataSource::GetCharMetrics(CharMetricsInfo &adv_info,const u32char &ch)
    {
        FontBitmap *bmp=GetCharBitmap(ch);

        if(!bmp)
            return false;

        adv_info=bmp->metrics_info;
        return(true);
    }
}//namespace hgl::graph
