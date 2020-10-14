#ifndef HGL_GUI_THEME_ENGINE_INCLUDE
#define HGL_GUI_THEME_ENGINE_INCLUDE

#include<hgl/type/RectScope.h>
namespace hgl
{
    namespace gui
    {
        class Widget;

        class ThemeEngine
        {        
        public:

            virtual bool Init()=0;
            virtual void Clear()=0;

            virtual void DrawFrame(const Widget *)=0;
            //virtual void DrawButton(const Widget *)=0;
            //virtual void DrawCheckBox(const Widget *)=0;
            //virtual void DrawRadioBox(const Widget *)=0;
        };//class ThemeEngine

//        ThemeEngine *CreateThemeEngine();                   
        ThemeEngine *GetDefaultThemeEngine();                                   ///<获取缺省主题引擎
    }//namespace gui
}//namespace hgl
#endif//HGL_GUI_THEME_ENGINE_INCLUDE