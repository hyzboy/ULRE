#include<hgl/graph/font/FontSource.h>

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

        for(auto &fsp:source_map)
        {
            if(fsp->value)
                fsp->value->RefRelease(this);
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

        const int count=source_map.GetCount();

        auto **fsp=source_map.GetDataList();

        for(int i=0;i<count;i++)
        {
            get_max(max_char_height,(*fsp)->value->GetCharHeight());

            ++fsp;
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

        if(source_map.ContainsValue(fs))
        {
            const bool refresh=(fs->GetCharHeight()==max_char_height);

            fs->RefRelease(this);
            source_map.DeleteByValue(fs);

            if(refresh)
                RefreshMaxCharHeight();
        }
    }
        
    FontDataSource *FontSource::GetFontDataSource(const u32char &ch)
    {
        if(hgl::is_space(ch))return(nullptr);	//不能显示的数据或是空格

        const auto count=source_map.GetCount();

        if(count>0)
        {
            auto **fsp=source_map.GetDataList();

            for(int i=0;i<count;i++)
            {
                if(IsInUnicodeBlock((*fsp)->key,ch))
                    return (*fsp)->value;

                ++fsp;
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

    const	CLA *FontSource::GetCLA(const u32char &ch)
    {
        FontDataSource *s=GetFontDataSource(ch);

        if(!s)
            return(default_source->GetCLA(ch));

        return s->GetCLA(ch);
    }
}//namespace hgl::graph
