#ifndef HGL_GRAPH_WINDOW_INCLUDE
#define HGL_GRAPH_WINDOW_INCLUDE

#include<hgl/type/BaseString.h>
namespace hgl
{
    namespace graph
    {
        class Window
        {
        protected:

            uint width,height;

            OSString win_name;

        public:

            uint GetWidth()const{return width;}
            uint GetHeight()const{return height;}

        public:

            Window(const OSString &wn)
            {
                width=height=0;
                win_name=wn;
            }
            virtual ~Window()=default;

            virtual bool Create(uint,uint)=0;
            virtual bool Create(uint,uint,uint)=0;
            virtual void Close()=0;

            virtual void Show()=0;
            virtual void Hide()=0;
        };//class Window

        Window *CreateRenderWindow(const OSString &win_name);
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_WINDOW_INCLUDE
