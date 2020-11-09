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

        ThemeForm *DefaultThemeEngine::CreateForm(Form *f,RenderTarget *rt,RenderCmdBuffer *rc)
        {
            return(new DefaultThemeForm(f,rt,rc));
        }

        void DefaultThemeEngine::DrawPanel(RenderCmdBuffer *rc,const RectScope2f &rs)
        {
            if(!rc)return;


        }
    }//namespace gui
}//namespace hgl
