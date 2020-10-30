#ifndef HGL_GUI_DEFAULT_THEME_FORM_INCLUDE
#define HGL_GUI_DEFAULT_THEME_FORM_INCLUDE

#include<hgl/gui/ThemeForm.h>
#include<hgl/graph/VK.h>

namespace hgl
{    
    namespace gui
    {
        using namespace hgl::graph;

        class Form;

        class DefaultThemeForm:public ThemeForm
        {

        public:

            using ThemeForm::ThemeForm;
            ~DefaultThemeForm()=default;

            bool Render() override;
        };//class DefaultThemeForm
    }//namespace gui
}//namespace hgl
#endif//HGL_GUI_DEFAULT_THEME_FORM_INCLUDE
