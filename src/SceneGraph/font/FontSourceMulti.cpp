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

			source_map.Update(ub,fs);
		}
		
		void FontSourceMulti::Remove(UnicodeBlock ub)
		{
			FontSourcePointer fsp;

			if(source_map.Get(ub,fsp))
			{
				fsp->RefRelease(this);
				source_map.DeleteByKey(ub);
			}
		}

		void FontSourceMulti::Remove(FontSource *fs)
		{
			if(!fs)return;
			if(fs==default_source)return;

			if(source_map.ValueExist(fs))
			{
				fs->RefRelease(this);
				source_map.DeleteByValue(fs);
			}
		}

		FontBitmap *FontSourceMulti::GetCharBitmap(const u32char &ch)
		{
			if(hgl::isspace(ch))return(nullptr);	//不能显示的数据或是空格

			const auto count=source_map.GetCount();
			auto **fsp=source_map.GetDataList();

			for(int i=0;i<count;i++)
			{
				if(IsInUnicodeBlock((*fsp)->left,ch))
					return (*fsp)->right->GetCharBitmap(ch);

				++fsp;
			}

			if(default_source)
				return default_source->GetCharBitmap(ch);

			return nullptr;
		}
	}//namespace graph
}//namespace hgl
