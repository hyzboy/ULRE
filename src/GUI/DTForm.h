#ifndef HGL_GUI_DEFAULT_THEME_FORM_INCLUDE
#define HGL_GUI_DEFAULT_THEME_FORM_INCLUDE

#include<hgl/gui/ThemeForm.h>
#include<hgl/graph/VK.h>
using namespace hgl::graph;

namespace hgl
{    
    namespace gui
    {
        class Form;

        namespace default_theme
        {
            class DTForm:public ThemeForm
            {

            public:

                DTForm(Form *f):ThemeForm(f){}
                ~DTForm()=default;
            };//class DTForm
        }//namespace default_theme
    }//namespace gui
}//namespace hgl
#endif//HGL_GUI_DEFAULT_THEME_FORM_INCLUDE