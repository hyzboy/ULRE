#include"DefaultThemeEngine.h"

namespace hgl
{
    namespace gui
    {
        ThemeEngine *CreateDefaultThemeEngine(VulkanDevice *dev)
        {
            return(new DefaultThemeEngine(dev));
        }

        bool DefaultThemeEngine::Init()
        {
            return(true);
        }

        DefaultThemeEngine::~DefaultThemeEngine()
        {
            Clear();
        }

        void DefaultThemeEngine::Clear()
        {
            for(auto &pair:form_list)
            {
                if(pair.second)
                    delete pair.second;
            }

            form_list.Clear();
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
