#include<hgl/graph/font/FontSource.h>

namespace hgl
{
    namespace graph
    {
        void FontSource::RefAcquire(void *ptr)
        {
            if(!ptr)return;

            ref_object.Add(ptr);

            return;
        }

        void FontSource::RefRelease(void *ptr)
        {
            if(!ptr)return;

            ref_object.Delete(ptr);
        }

        namespace
        {
            constexpr u16char   BeginSymbols    []=U16_TEXT("!),❟.:;?]}¨·ˇˉ―‖’❜”„❞…∶、。〃々❯〉》」』】〕〗！＂＇），．：；？］｀｜｝～»›");     //行首禁用符号
            constexpr u16char 	EndSymbols      []=U16_TEXT("([{·❛‘“‟❝❮〈《「『【〔〖（．［｛«‹");                                           //行尾禁用符号
            constexpr u16char 	CurrencySymbols []=U16_TEXT("₳฿₿￠₡¢₢₵₫€￡£₤₣ƒ₲₭Ł₥₦₽₱＄$₮ℳ₶₩￦¥￥₴₸¤₰៛₪₯₠₧﷼㍐원৳₹₨৲௹");                //货币符号
            constexpr u16char   VRotateSymbols  []=U16_TEXT("()[]{}〈〉《》「」『』【】〔〕〖〗（）［］｛｝―‖…∶｜～");                        //竖排必须旋转的符号

            constexpr int       BeginSymbolsCount   =(sizeof(BeginSymbols)   /sizeof(u16char))-1;
            constexpr int 		EndSymbolsCount	    =(sizeof(EndSymbols)     /sizeof(u16char))-1;
            constexpr int 		CurrencySymbolsCount=(sizeof(CurrencySymbols)/sizeof(u16char))-1;
            constexpr int       VRotateSymbolsCount =(sizeof(VRotateSymbols) /sizeof(u16char))-1;

            MapObject<u32char,CharAttributes> all_char_attrs;
        }//namespace
        
        const CLA *FontSource::GetCLA(const u32char &ch)
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

                    if(!attr->end_disable)       //货币符号同样行尾禁用
                    attr->end_disable   =hgl::strchr(CurrencySymbols,   ch,CurrencySymbolsCount );

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
