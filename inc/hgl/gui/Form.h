#ifndef HGL_GUI_FORM_INCLUDE
#define HGL_GUI_FORM_INCLUDE

#include<hgl/graph/VKPipeline.h>
#include<hgl/gui/Widget.h>
namespace hgl
{
    namespace gui
    {
        using namespace hgl::graph;

        /**
         * 窗体组件，窗体是承载所有GUI控件的基本装置
         */
        class Form:public Widget
        {
        protected:  //每个窗体独立一个FBO存在，所以每个窗体会有自己的RenderTarget与pipeline

        public:

            Form(ThemeEngine *te=nullptr):Widget(nullptr,te){}
            virtual ~Form()=default;
        };//class Form
    }//namespace gui
}//namespace hgl
#endif//HGL_GUI_FORM_INCLUDE
