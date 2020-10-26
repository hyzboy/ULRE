#ifndef HGL_GUI_WIDGET_INCLUDE
#define HGL_GUI_WIDGET_INCLUDE

#include<hgl/type/RectScope.h>
#include<hgl/math/Vector.h>
namespace hgl
{
    namespace gui
    {
        class ThemeEngine;

        enum Alignment
        {
            None    =0x00,
            Left    =0x01,
            Top     =0x02,
            Right   =0x04,
            Bottom  =0x08,
            Center  =0x10,
            HCenter =0x20,
            VCenter =0x40
        };//enum Alignment

        /**
         * 装置类，是所有GUI组件的基类
         */
        class Widget
        {
        private:

            Widget *parent_widget;
            ThemeEngine *theme_engine;

            bool visible;                                                       ///<控件是否可看见
            bool recv_event;                                                    ///<控件是否接收事件
            uint8 align_bits;                                                   ///<对齐位属性(注：不可见依然影响排版)

            RectScope2f position;                                               ///<所在位置

        public:

            const bool          IsVisible   ()const{return visible;}
            const bool          IsRecvEvent ()const{return recv_event;}
            const uint8         GetAlign    ()const{return align_bits;}
            const RectScope2f & GetPosition ()const{return position;}
            const Vector2f      GetOffset   ()const{return position.GetLeftTop();}
            const Vector2f      GetSize     ()const{return position.GetSize();}

            void                SetVisible  (const bool);
            void                SetRecvEvent(const bool);
            void                SetAlign    (const uint8);
            void                SetPosition (const RectScope2i &);
            void                SetSize     (const Vector2f &);

        public: //事件

            virtual void OnResize(uint,uint);

        public:

            Widget(Widget *parent=nullptr,ThemeEngine *te=nullptr);
            virtual ~Widget()=default;
        };//class Widget
    }//namespace gui
}//namespace hgl
#endif//HGL_GUI_WIDGET_INCLUDE
