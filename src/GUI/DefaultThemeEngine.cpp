#include"DefaultThemeEngine.h"

namespace hgl
{
    namespace gui
    {
        ThemeEngine *CreateDefaultThemeEngine(vulkan::GPUDevice *dev)
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

            bool DefaultThemeEngine::Registry(Form *f)
            {
                if(!f)return(false);
                if(form_list.KeyExist(f))return(false);

            
                return(true);
            }

            void DefaultThemeEngine::Unregistry(Form *f)
            {
                if(!f)return;
                if(!form_list.KeyExist(f))return;
            }
        
            void DefaultThemeEngine::Render(Form *f)
            {
            }
        }//namespace default_theme
    }//namespace gui
}//namespace hgl