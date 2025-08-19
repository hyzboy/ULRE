#include<hgl/graph/font/FontSource.h>

namespace hgl::graph
{        
    IFontSource *CreateFontSource(const Font &f);        //各平台独立提供

    static ObjectMap<Font,IFontSource> FontStorage;

    IFontSource *AcquireFontSource(const Font &f)
    {
        IFontSource *source;

        if(!FontStorage.Get(f,source))
        {
            source=CreateFontSource(f);

            FontStorage.Add(f,source);
        }

        //source->RefAcquire();

        return source;
    }

    IFontSource *CreateCJKFontSource(const os_char *latin_fontname,const os_char *cjk_fontname,const uint32_t size)
    {
        Font latin_fnt(latin_fontname,0,size);
        Font cjk_fnt(cjk_fontname,0,size);

        IFontSource *latin_fs=AcquireFontSource(latin_fnt);
        IFontSource *cjk_fs=AcquireFontSource(cjk_fnt);

        if(!latin_fs||!cjk_fs)
            return(nullptr);

        FontSourceMulti *font_source=new FontSourceMulti(latin_fs);

        font_source->AddCJK(cjk_fs);

        return font_source;
    }

    IFontSource *AcquireFontSource(const os_char *name,const uint32_t size)
    {
        Font fnt(name,0,size);

        return AcquireFontSource(fnt);
    }

    void ReleaseFontSource(IFontSource *fs)
    {
        if(!fs)return;


    }
}//namespace hgl::graph
