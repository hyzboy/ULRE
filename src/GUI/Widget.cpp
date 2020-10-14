#include<hgl/gui/Widget.h>
#include<hgl/gui/ThemeEngine.h>

namespace hgl
{
    namespace gui
    {
        Widget::Widget(Widget *parent=nullptr,ThemeEngine *te=nullptr)
        {
            parent_widget=parent;
            
            if(te)
                theme_engine=te;
            else
                theme_engine=GetDefaultThemeEngine();

                                //默认值
            visible=false;      //不显示
            recv_event=false;   //不接收事件
            align_bits=0;       //不对齐
            position.Clear();
        }
    }//namespace gui
}//namespace hgl