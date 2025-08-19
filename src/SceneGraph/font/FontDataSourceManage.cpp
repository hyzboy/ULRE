#include<hgl/graph/font/FontSource.h>

namespace hgl::graph
{        
    FontDataSource *CreateFontSource(const Font &f);        //各平台独立提供

    static ObjectMap<Font,FontDataSource> FontStorage;

    FontDataSource *AcquireFontDataSource(const Font &f)
    {
        FontDataSource *source;

        if(!FontStorage.Get(f,source))
        {
            source=CreateFontSource(f);

            FontStorage.Add(f,source);
        }

        //source->RefAcquire();

        return source;
    }

    FontDataSource *AcquireFontDataSource(const os_char *name,const uint32_t size)
    {
        Font fnt(name,0,size);

        return AcquireFontDataSource(fnt);
    }

    void ReleaseFontDataSource(FontDataSource *fs)
    {
        if(!fs)return;
    }

    FontSource *CreateCJKFontSource(const os_char *latin_fontname,const os_char *cjk_fontname,const uint32_t size)
    {
        Font latin_fnt(latin_fontname,0,size);
        Font cjk_fnt(cjk_fontname,0,size);

        FontDataSource *latin_fs=AcquireFontDataSource(latin_fnt);
        FontDataSource *cjk_fs=AcquireFontDataSource(cjk_fnt);

        if(!latin_fs||!cjk_fs)
            return(nullptr);

        FontSource *font_source=new FontSource(latin_fs);

        font_source->AddCJK(cjk_fs);

        return font_source;
    }
}//namespace hgl::graph
