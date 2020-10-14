#include<hgl/gui/ThemeEngine.h>

namespace hgl
{
    namespace gui
    {
        namespace
        {
            ThemeEngine *default_theme_engine=nullptr;
        }//namespace

        ThemeEngine *CreateDefaultThemeEngine();

        ThemeEngine *GetDefaultThemeEngine()
        {
            if(!default_theme_engine)
                default_theme_engine=CreateDefaultThemeEngine();

            return default_theme_engine;
        }

        ThemeEngine *CreateThemeEngine()
        {
            return GetDefaultThemeEngine();
        }
    }//namespace gui
}//namespace hgl