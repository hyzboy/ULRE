#include<hgl/graph/font/FontSource.h>
namespace hgl
{
    namespace graph
    {        
        FontSource *CreateFontSource(const Font &f);        //各平台独立提供

        static ObjectMap<Font,FontSource> FontStorage;

        FontSource *AcquireFontSource(const Font &f)
        {
            FontSource *source;

            if(!FontStorage.Get(f,source))
            {
                source=CreateFontSource(f);

                FontStorage.Add(f,source);
            }

            return source;
        }
    }//namespace graph
}//namespace hgl
