#include"DefaultThemeEngine.h"

namespace hgl
{
    namespace gui
    {
        ThemeEngine *CreateDefaultThemeEngine()
        {
            return(new DefaultThemeEngine);
        }

        bool DefaultThemeEngine::Init()
        {
        }

        void DefaultThemeEngine::Clear()
        {
        }

        void DefaultThemeEngine::DrawFrame(const Widget *w)
        {
        }
    }//namespace gui
}//namespace hgl