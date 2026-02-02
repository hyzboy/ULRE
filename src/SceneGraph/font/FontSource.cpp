#include<hgl/graph/font/FontSource.h>
#include<vector>

namespace hgl::graph
{
    FontSource::FontSource(FontDataSource *fs)
    {
        default_source=fs;

        if(fs)
        {
            fs->RefAcquire(this);

            max_char_height=fs->GetCharHeight();
        }
        else
            max_char_height=0;
    }

    FontSource::~FontSource()
    {
        if(default_source)
            default_source->RefRelease(this);

        std::vector<FontSourcePointer> values;
        source_map.GetValueArray(values);
        for(auto *fsp:values)
        {
            if(fsp)
                fsp->RefRelease(this);
        }
    }

    void FontSource::Add(UnicodeBlock ub,FontDataSource *fs)
    {
        if(ub<UnicodeBlock::BEGIN_RANGE
            ||ub>UnicodeBlock::END_RANGE
            ||!fs
            ||fs==default_source)return;

        get_max(max_char_height,fs->GetCharHeight());

        source_map.ChangeOrAdd(ub,fs);
    }

    void FontSource::RefreshMaxCharHeight()
    {
        max_char_height=0;

        std::vector<FontSourcePointer> values;
        source_map.GetValueArray(values);
        for(auto *fsp:values)
        {
            if(fsp)
                get_max(max_char_height, fsp->GetCharHeight());
        }
    }

    void FontSource::Remove(UnicodeBlock ub)
    {
        FontSourcePointer fsp;

        if(source_map.Get(ub,fsp))
        {
            const bool refresh=(fsp->GetCharHeight()==max_char_height);

            fsp->RefRelease(this);
            source_map.DeleteByKey(ub);

            if(refresh)
                RefreshMaxCharHeight();
        }
    }

    void FontSource::Remove(FontDataSource *fs)
    {
        if(!fs)return;
        if(fs==default_source)return;

        // 查找是否存在此FontDataSource
        bool found=false;
        std::vector<UnicodeBlock> keys;
        source_map.GetKeyArray(keys);
        for(const auto &key:keys)
        {
            auto *value=source_map.GetValuePointer(key);
            if(value && *value==fs)
            {
                found=true;
                break;
            }
        }

        if(found)
        {
            const bool refresh=(fs->GetCharHeight()==max_char_height);

            fs->RefRelease(this);
            
            // 删除所有值为fs的项
            for(const auto &key:keys)
            {
                auto *value=source_map.GetValuePointer(key);
                if(value && *value==fs)
                {
                    source_map.DeleteByKey(key);
                    break;
                }
            }

            if(refresh)
                RefreshMaxCharHeight();
        }
    }

    FontDataSource *FontSource::GetFontDataSource(const u32char &ch)
    {
        if(hgl::is_space(ch))return(nullptr);   //不能显示的数据或是空格

        if(!source_map.IsEmpty())
        {
            std::vector<UnicodeBlock> keys;
            source_map.GetKeyArray(keys);
            for(const auto &key:keys)
            {
                if(IsInUnicodeBlock(key,ch))
                {
                    auto *value=source_map.GetValuePointer(key);
                    if(value)
                        return *value;
                }
            }
        }

        return default_source;
    }

    FontBitmap *FontSource::GetCharBitmap(const u32char &ch)
    {
        FontDataSource *s=GetFontDataSource(ch);

        if(!s)
            return(nullptr);

        return s->GetCharBitmap(ch);
    }

    const bool FontSource::GetCharMetrics(CharMetricsInfo &cmi,const u32char &ch)
    {
        FontDataSource *s=GetFontDataSource(ch);

        if(!s)
            return(0);

        return s->GetCharMetrics(cmi,ch);
    }

    const   CLA *FontSource::GetCLA(const u32char &ch)
    {
        FontDataSource *s=GetFontDataSource(ch);

        if(!s)
            return(default_source->GetCLA(ch));

        return s->GetCLA(ch);
    }
}//namespace hgl::graph
