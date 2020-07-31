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
        }//namespace
		
		const CLA *FontSource::GetCLA(const u32char &ch)
		{
            CLA *cla;

            if(cla_cache.Get(ch,cla))
                return cla;

            cla=new CLA;

            cla->ch=ch;

            if(hgl::isspace(ch))
            {
                cla->visible=false;
            }
            else
            {
                cla->visible=true;

                cla->begin_disable  =hgl::strchr(BeginSymbols,      ch,BeginSymbolsCount    );
                cla->end_disable    =hgl::strchr(EndSymbols,        ch,EndSymbolsCount      );

                if(!cla->end_disable)       //货币符号同样行尾禁用
                cla->end_disable    =hgl::strchr(CurrencySymbols,   ch,CurrencySymbolsCount );

                cla->vrotate        =hgl::strchr(VRotateSymbols,    ch,VRotateSymbolsCount  );

                cla->is_cjk         =isCJK(ch);
                cla->is_emoji       =isEmoji(ch);

                cla->is_punctuation =isPunctuation(ch);
                        
                if(!GetCharMetrics(cla->adv_info,ch))
                    hgl_zero(cla->adv_info);
                else
                if(cla->adv_info.w>0&&cla->adv_info.h>0)
                {
                    cla->size=ceil(cla->adv_info.adv_x);
                }
            }

            cla_cache.Add(ch,cla);
            return cla;
		}
    }//namespace graph
}//namespace hgl
