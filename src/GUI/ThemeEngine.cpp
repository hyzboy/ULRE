#include<hgl/gui/ThemeEngine.h>
#include<hgl/gui/ThemeForm.h>

namespace hgl
{
    namespace gui
    {
        namespace
        {
            ThemeEngine *default_theme_engine=nullptr;
        }//namespace

        ThemeEngine *CreateDefaultThemeEngine(vulkan::GPUDevice *dev);

        ThemeEngine *GetDefaultThemeEngine(vulkan::GPUDevice *dev)
        {
            if(!default_theme_engine)
                default_theme_engine=CreateDefaultThemeEngine(dev);

            return default_theme_engine;
        }

        ThemeEngine *CreateThemeEngine(vulkan::GPUDevice *dev)
        {
            return GetDefaultThemeEngine();
        }
            
        bool ThemeEngine::Resize(Form *f,const uint32_t w,const uint32_t h)
        {
            if(!f)return(false);

            ThemeForm *tf;

            if(!form_list.Get(f,tf))return(false);

            if(w<=0||h<=0)
            {
                tf->SetRenderTarget(nullptr);
                return(true);
            }

        }
    }//namespace gui
}//namespace hgl