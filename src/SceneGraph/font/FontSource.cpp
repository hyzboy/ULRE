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
    }//namespace graph
}//namespace hgl
