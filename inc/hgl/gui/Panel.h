#ifndef HGL_GUI_PANEL_INCLUDE
#define HGL_GUI_PANEL_INCLUDE

#include<hgl/gui/Widget.h>
namespace hgl
{
    namespace gui
    {
        class Panel:public Widget
        {
        public:

            Panel(Widget *p):Widget(p){}

            virtual ~Panel()=default;

            void Draw() override;
        };//class Panel:public Widget
    }//namespace gui
}//namespace hgl
#endif//HGL_GUI_PANEL_INCLUDE
