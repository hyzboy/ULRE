#include<hgl/graph/font/FontSource.h>
#include<hgl/util/sdf.h>
#include<cstring>
#include<vector>

namespace hgl::graph
{
    void FontBitmapDataSource::SetSDFEnabled(bool enabled)
    {
        if(sdf_enabled==enabled)return;

        sdf_enabled=enabled;

        //位图缓存已陈旧(开关切换后位图内容/尺寸都会变化)，清空并释放内存，下次按需重建
        std::vector<FontBitmap *> bmps;
        chars_bitmap.GetValueArray(bmps);
        for(auto *bmp:bmps)
        {
            if(bmp)
            {
                delete[] bmp->data;
                delete bmp;
            }
        }
        chars_bitmap.Clear();

        //排版信息缓存中的 metrics 同样陈旧，一并释放重建(其中的 attr 为全局共享，不释放)
        std::vector<CLA *> clas;
        cla_cache.GetValueArray(clas);
        for(auto *cla:clas)
        {
            delete cla;
        }
        cla_cache.Clear();
    }

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
                // SDF 转换：将灰度位图转为距离场，四周扩展 TEXT_SDF_SPREAD 象素保证边缘距离场完整，
                // 勾边(outline)也依赖这部分额外空间。排版信息相应外扩，adv_x/adv_y 同步补偿 2P 以避免布局重叠。
                int32_t w = bmp->metrics_info.w;
                int32_t h = bmp->metrics_info.h;

                if(w > 0 && h > 0)
                {
                    const int32_t P  = TEXT_SDF_SPREAD;
                    const int32_t pw = w + 2 * P;
                    const int32_t ph = h + 2 * P;

                    // 分配放大源缓冲，填 0(背景)，字形位图居中拷贝到偏移 (P, P) 处
                    uint8_t *padded_src = new uint8_t[pw * ph];
                    memset(padded_src, 0, pw * ph);

                    for(int32_t y = 0; y < h; ++y)
                        memcpy(padded_src + (y + P) * pw + P, bmp->data + y * w, w);

                    int32_t temp_size = sdf_temp_buffer_size(pw, ph);

                    uint8_t *sdf_dst = new uint8_t[pw * ph];
                    void *temp = new uint8_t[temp_size];

                    sdf_generate(pw, ph, P, padded_src, sdf_dst, temp);

                    // 结果拷回位图(重分配为放大尺寸)
                    delete[] bmp->data;
                    bmp->data = new uint8_t[pw * ph];
                    memcpy(bmp->data, sdf_dst, pw * ph);

                    // 更新绘制信息：尺寸外扩 2P，偏移相应回退
                    // 注意：adv_x/adv_y 保持原始值，不随 SDF padding 调整。
                    // SDF padding 仅为距离场计算预留空间，字形视觉扩展由 TextLayout 的
                    // extra_advance 机制根据 bold/outline 动态补偿。
                    bmp->metrics_info.x -= P;
                    bmp->metrics_info.y -= P;
                    bmp->metrics_info.w  = pw;
                    bmp->metrics_info.h  = ph;

                    delete[] padded_src;
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
