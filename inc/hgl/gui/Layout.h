#ifndef HGL_GUI_LAYOUT_INCLUDE
#define HGL_GUI_LAYOUT_INCLUDE

#include<hgl/gui/Widget.h>
#include<hgl/type/SortedSet.h>
namespace hgl
{
    namespace gui
    {
        /**
         * 布局器基础
         */
        class LayoutBase
        {
        protected:

            SortedSet<Widget *> widgets_set;

        public:

            virtual bool AddWidget(Widget *);
            virtual bool RemoveWidget(Widget *);
        };//class LayoutBase

        /**
         * 布局器
         */
        class Layout:public LayoutBase
        {
        public:
        };//class Layout:public LayoutBase

        /**
         * 垂直分布布局器
         */
        class VBoxLayout:public LayoutBase
        {
        public:
        };//class VBoxLayout:public LayoutBase 

        /**
         * 水平分布布局器
         */
        class HBoxLayout:public LayoutBase
        {
        public:
        };//class HBoxLayout:public LayoutBase

        /**
         * 网格分布布局器
         */
        class GridLayout:public LayoutBase
        {
        public:
        };//class GridLayout:public LayoutBase
    }//namespace gui
}//namespace hgl
#endif//HGL_GUI_LAYOUT_INCLUDE
