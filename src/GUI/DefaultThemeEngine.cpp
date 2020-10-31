#include"DefaultThemeEngine.h"

namespace hgl
{
    namespace gui
    {
        ThemeEngine *CreateDefaultThemeEngine(GPUDevice *dev)
        {
            return(new DefaultThemeEngine(dev));
        }

        bool DefaultThemeEngine::Init()
        {
            return(true);
        }

        void DefaultThemeEngine::Clear()
        {
        }

        ThemeForm *DefaultThemeEngine::CreateForm(Form *f,RenderTarget *rt,RenderCommand *rc)
        {
            return(new DefaultThemeForm(f,rt,rc));
        }
    }//namespace gui
}//namespace hgl
