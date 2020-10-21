#ifndef HGL_GUI_THEME_FORM_INCLUDE
#define HGL_GUI_THEME_FORM_INCLUDE

#include<hgl/graph/VKRenderTarget.h>
#include<hgl/gui/Form.h>

namespace hgl
{
    namespace gui
    {
        class ThemeForm
        {
        protected:
        
            Form *form;
            hgl::graph::RenderTarget *render_target;

        public:

            ThemeForm(Form *);

            void SetRenderTarget(hgl::graph::RenderTarget *);
        };//class ThemeForm
    }//namespace gui
}//namespace hgl
#endif//HGL_GUI_THEME_FORM_INCLUDE
