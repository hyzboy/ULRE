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

        // 覆盖旧数据源时：先摘除本项引用；若旧源不再被任何项引用，对称释放（RefObject 以指针去重，多块共享同一源只计一次）
        FontSourcePointer old;
        if(source_map.Get(ub,old))
        {
            if(old==fs)
                return;     // 已注册于本块（引用已计），幂等返回

            source_map.DeleteByKey(ub);

            if(old && old!=default_source && !IsSourceReferenced(old))
                old->RefRelease(this);
        }

        get_max(max_char_height,fs->GetCharHeight());

        fs->RefAcquire(this);   // 与析构/Remove 的 RefRelease 对称；OrderedSet 去重，多块重复注册只计一次
        source_map[ub] = fs;
    }

    bool FontSource::IsSourceReferenced(FontSourcePointer fs)
    {
        if(!fs)
            return(false);

        if(fs==default_source)
            return(true);

        std::vector<FontSourcePointer> values;
        source_map.GetValueArray(values);
        for(auto *fsp:values)
        {
            if(fsp==fs)
                return(true);
        }

        return(false);
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

            source_map.DeleteByKey(ub);

            // 同一数据源可能仍被其它块引用 ⇒ 仅当它不再出现在任何项时才释放本 FontSource 的引用
            if(fsp && !IsSourceReferenced(fsp))
                fsp->RefRelease(this);

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
            if(source_map.ContainsKey(key))
            {
                const auto &value=source_map.GetValueRef(key);
                if(value==fs)
                {
                    found=true;
                    break;
                }
            }
        }

        if(found)
        {
            const bool refresh=(fs->GetCharHeight()==max_char_height);

            // 删除所有值为fs的项：同一数据源可注册到多个 Unicode 块（如 AddCJK 十余块），
            // 只删首个匹配会在 source_map 里留下悬空引用
            for(const auto &key:keys)
            {
                FontSourcePointer val;
                if(source_map.Get(key,val) && val==fs)
                    source_map.DeleteByKey(key);
            }

            fs->RefRelease(this);

            if(refresh)
                RefreshMaxCharHeight();
        }
    }

    void FontSource::SetSDFEnabled(bool enabled)
    {
        if(default_source)
            default_source->SetSDFEnabled(enabled);

        std::vector<FontSourcePointer> values;
        source_map.GetValueArray(values);
        for(auto *fsp:values)
        {
            if(fsp)
                fsp->SetSDFEnabled(enabled);
        }
    }

    bool FontSource::IsSDFEnabled()const
    {
        return default_source?default_source->IsSDFEnabled():false;
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
                    if(source_map.ContainsKey(key))
                        return source_map.GetValueRef(key);
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
            s=default_source;         //空格等无归属字符回落缺省源

        if(!s)
            return(nullptr);          //无任何数据源：防御性返回，原先直接解引用空 default_source 会崩溃

        return s->GetCLA(ch);
    }
}//namespace hgl::graph
