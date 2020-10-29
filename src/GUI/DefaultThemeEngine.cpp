#include"DefaultThemeEngine.h"

namespace hgl
{
    namespace gui
    {
        ThemeEngine *CreateDefaultThemeEngine(GPUDevice *dev)
        {
            return(new default_theme::DefaultThemeEngine(dev));
        }

        namespace default_theme
        {
            bool DefaultThemeEngine::Init()
            {
                return(true);
            }

            void DefaultThemeEngine::Clear()
            {
            }

            ThemeForm *DefaultThemeEngine::CreateForm(Form *f,RenderTarget *rt,RenderCommand *rc)
            {
                return(new DTForm(f,rt,rc));
            }
        }//namespace default_theme
    }//namespace gui
}//namespace hgl