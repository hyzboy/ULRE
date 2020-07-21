#include<hgl/graph/font/FontSource.h>

namespace hgl
{
	namespace graph
	{	
        FontSourceMulti::FontSourceMulti(FontSource *fs)
		{
			default_source=fs;

			if(fs)
				fs->RefAcquire(this);

			max_char_height=fs->GetCharHeight();
		}

		FontSourceMulti::~FontSourceMulti()
		{
			if(default_source)
				default_source->RefRelease(this);
		}

		void FontSourceMulti::Add(UnicodeBlock ub,FontSource *fs)
		{
			if(ub<UnicodeBlock::BEGIN_RANGE
			 ||ub>UnicodeBlock::END_RANGE
			 ||!fs
			 ||fs==default_source)return;

			 if(fs->GetCharHeight()>max_char_height)
				max_char_height=fs->GetCharHeight();

			source_map.Update(ub,fs);
		}

		void FontSourceMulti::RefreshMaxCharHeight()
		{
			max_char_height=0;

			const int count=source_map.GetCount();

			const auto **fsp=source_map.GetDataList();

			for(int i=0;i<count;i++)
			{
				if((*fsp)->right->GetCharHeight()>max_char_height)
					max_char_height=(*fsp)->right->GetCharHeight();

				++fsp;
			}
		}
		
		void FontSourceMulti::Remove(UnicodeBlock ub)
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

		void FontSourceMulti::Remove(FontSource *fs)
		{
			if(!fs)return;
			if(fs==default_source)return;

			if(source_map.ValueExist(fs))
			{
				const bool refresh=(fs->GetCharHeight()==max_char_height);

				fs->RefRelease(this);
				source_map.DeleteByValue(fs);

				if(refresh)
					RefreshMaxCharHeight();
			}
		}
		
		FontSource *FontSourceMulti::GetFontSource(const u32char &ch)
		{
			if(hgl::isspace(ch))return(nullptr);	//不能显示的数据或是空格

			const auto count=source_map.GetCount();

			if(count>0)
			{
				auto **fsp=source_map.GetDataList();

				for(int i=0;i<count;i++)
				{
					if(IsInUnicodeBlock((*fsp)->left,ch))
						return (*fsp)->right;

					++fsp;
				}
			}

			return default_source;
		}

		FontBitmap *FontSourceMulti::GetCharBitmap(const u32char &ch)
		{
			FontSource *s=GetFontSource(ch);

			if(!s)
				return(nullptr);

			return s->GetCharBitmap(ch);
		}

		int FontSourceMulti::GetCharAdvWidth(const u32char &ch)
		{
			FontSource *s=GetFontSource(ch);

			if(!s)
				return(0);

			return s->GetCharAdvWidth(ch);
		}
	}//namespace graph
}//namespace hgl
