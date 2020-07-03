#ifndef HGL_GRAPH_FONT_MULTI_SOURCE_INCLUDE
#define HGL_GRAPH_FONT_MULTI_SOURCE_INCLUDE

#include<hgl/graph/font/FontSource.h>
#include<hgl/type/UnicodeBlocks.h>
namespace hgl
{
    namespace graph
    {
        class FontMultiSource:public FontSource
        {
            using FontSourcePointer=FontSource *;
            using FontSourceTable=FontSourcePointer[(size_t)UnicodeBlock::RANGE_SIZE];

            FontSourceTable source_map;

        public:

            FontMultiSource();
            virtual ~FontMultiSource();

            void Add(UnicodeBlock,FontSource *);
        };//class FontMultiSource:public FontSource
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_FONT_MULTI_SOURCE_INCLUDE
