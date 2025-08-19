#include<hgl/graph/font/FontSource.h>

namespace hgl
{
    namespace graph
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
            constexpr u32char   VRotateSymbols  []=U32_TEXT("()[]{}〈〉《》「」『』【】〔〕〖〗（）［］｛｝―‖…∶｜～");                        //竖排必须旋转的符号

            constexpr int       BeginSymbolsCount   =(sizeof(BeginSymbols)   /sizeof(u32char))-1;
            constexpr int       EndSymbolsCount	    =(sizeof(EndSymbols)     /sizeof(u32char))-1;
            constexpr int       CurrencySymbolsCount=(sizeof(CurrencySymbols)/sizeof(u32char))-1;
            constexpr int       VRotateSymbolsCount =(sizeof(VRotateSymbols) /sizeof(u32char))-1;

            ObjectMap<u32char,CharAttributes> all_char_attrs;
        }//namespace

        const CLA *FontDataSource::GetCLA(const u32char &ch)
        {
            CLA *cla;

            if(cla_cache.Get(ch,cla))
                return cla;

            CharAttributes *attr;

            const int pos=all_char_attrs.GetValueAndSerial(ch,attr);

            if(pos<0)
            {
                attr=new CharAttributes;
                
                attr->ch=ch;

                attr->space=hgl::isspace(ch);

                if(!attr->space)
                {
                    attr->begin_disable =hgl::strchr(BeginSymbols,      ch,BeginSymbolsCount    );
                    attr->end_disable   =hgl::strchr(EndSymbols,        ch,EndSymbolsCount      );

                    attr->is_currency   =hgl::strchr(CurrencySymbols,   ch,CurrencySymbolsCount );

                    if(!attr->end_disable)       
                    attr->end_disable   =attr->is_currency;     //货币符号同样行尾禁用

                    attr->vrotate       =hgl::strchr(VRotateSymbols,    ch,VRotateSymbolsCount  );

                    attr->is_cjk        =isCJK(ch);
                    attr->is_emoji      =isEmoji(ch);

                    attr->is_punctuation=isPunctuation(ch);
                }

                all_char_attrs.Add(ch,attr);
            }

            cla=new CLA;
            cla->attr=attr;

            if(!attr->space)
            {                        
                if(!GetCharMetrics(cla->metrics,ch))
                {
                    cla->visible=false;
                    hgl_zero(cla->metrics);
                }
                else
                {
                    cla->visible=(cla->metrics.w>0&&cla->metrics.h>0);
                }
            }
            else
            {
                cla->visible=false;
            }

            cla_cache.Add(ch,cla);
            return cla;
        }
    }//namespace graph
}//namespace hgl
