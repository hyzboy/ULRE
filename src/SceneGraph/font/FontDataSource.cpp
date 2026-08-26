#include<hgl/graph/font/FontSource.h>

namespace hgl::graph
{
    void FontDataSource::RefAcquire(void *ptr)
    {
        if(!ptr)return;

        ref_object.Add(ptr);

        return;
    }

    void FontDataSource::RefRelease(void *ptr)
    {
        if(!ptr)return;

        ref_object.Delete(ptr);
    }

    namespace
    {
        constexpr u32char   BeginSymbols    []=U32_TEXT("!),❟.:;?]}¨·ˇˉ―‖’❜”„❞…∶、。〃々❯〉》」』】〕〗！＂＇），．：；？］｀｜｝～»›");  //行首禁用符号
        constexpr u32char   EndSymbols      []=U32_TEXT("([{·❛‘“‟❝❮〈《「『【〔〖（．［｛«‹");                                       //行尾禁用符号
        constexpr u32char   CurrencySymbols []=U32_TEXT("₳฿₿￠₡¢₢₵₫€￡£₤₣ƒ₲₭Ł₥₦₽₱＄$₮ℳ₶₩￦¥￥₴₸¤₰៛₪₯₠₧﷼㍐원৳₹₨৲௹");                //货币符号
        constexpr u32char   VRotateSymbols  []=U32_TEXT("()[]{}〈〉《》「」『』【】〔〕〖〗（）［］｛｝―‖…∶｜～\u2018\u2019\u201C\u201D\u201A\u201E\u2014\u2013\u2010\u2011\u2012\u2212\u00B7\u30FB$\u20AC\u00A5\u00A3\u00A2\u20B9=+\u00B1\u00D7\u00F7<>\u2264\u2265%\u2030#&@\u2190\u2192");                        //竖排必须旋转的符号

        constexpr int       BeginSymbolsCount   =(sizeof(BeginSymbols)   /sizeof(u32char))-1;
        constexpr int       EndSymbolsCount     =(sizeof(EndSymbols)     /sizeof(u32char))-1;
        constexpr int       CurrencySymbolsCount=(sizeof(CurrencySymbols)/sizeof(u32char))-1;
        constexpr int       VRotateSymbolsCount =(sizeof(VRotateSymbols) /sizeof(u32char))-1;

        UnorderedMap<u32char,CharAttributes *> all_char_attrs;
    }//namespace

    const CLA *FontDataSource::GetCLA(const u32char &ch)
    {
        CLA *char_draw_style;

        if(cla_cache.Get(ch,char_draw_style))
            return char_draw_style;

        CharAttributes *attr=nullptr;

        if(!all_char_attrs.Get(ch,attr))
        {
            attr=new CharAttributes;

            attr->ch=ch;

            attr->space=hgl::is_space(ch);

            if(!attr->space)
            {
                attr->begin_disable =hgl::strchr(BeginSymbols,      ch,BeginSymbolsCount    );
                attr->end_disable   =hgl::strchr(EndSymbols,        ch,EndSymbolsCount      );

                attr->is_currency   =hgl::strchr(CurrencySymbols,   ch,CurrencySymbolsCount );

                if(!attr->end_disable)
                attr->end_disable   =attr->is_currency;     //货币符号同样行尾禁用

                attr->vrotate       =hgl::strchr(VRotateSymbols,    ch,VRotateSymbolsCount  )
                                    ||hgl::isLatin(ch)||hgl::isCyrillic(ch)||hgl::isGreek(ch)
                                    ||hgl::isArabic(ch)||hgl::isHebrew(ch)
                                    ||hgl::isThai(ch)||hgl::isDevanagari(ch);

                attr->is_cjk        =isCJK(ch);
                attr->is_emoji      =isEmoji(ch);

                attr->is_punctuation=isPunctuation(ch);
            }

            all_char_attrs.Add(ch,attr);
        }

        char_draw_style=new CLA;
        char_draw_style->attr=attr;

        if(!attr->space)
        {
            if(!GetCharMetrics(char_draw_style->metrics,ch))
            {
                char_draw_style->visible=false;
                mem_zero(char_draw_style->metrics);
            }
            else
            {
                char_draw_style->visible=(char_draw_style->metrics.w>0&&char_draw_style->metrics.h>0);
            }
        }
        else
        {
            char_draw_style->visible=false;
        }

        cla_cache.Add(ch,char_draw_style);
        return char_draw_style;
    }
}//namespace hgl::graph
