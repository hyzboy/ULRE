#include<hgl/gui/ThemeEngine.h>
#include<hgl/gui/ThemeForm.h>
#include<hgl/graph/VKRenderTarget.h>
#include<hgl/graph/VKDevice.h>

namespace hgl
{
    namespace gui
    {
        namespace
        {
            ThemeEngine *default_theme_engine=nullptr;
        }//namespace

        ThemeEngine *CreateDefaultThemeEngine(GPUDevice *dev);

        ThemeEngine *GetDefaultThemeEngine(GPUDevice *dev)
        {
            if(!default_theme_engine)
                default_theme_engine=CreateDefaultThemeEngine(dev);

            return default_theme_engine;
        }

        ThemeEngine *CreateThemeEngine(GPUDevice *dev)
        {
            return GetDefaultThemeEngine();
        }

        RenderTarget *ThemeEngine::CreateRenderTarget(const uint32_t w,const uint32_t h,const VkFormat format)
        {
            const uint width=power_to_2(w);
            const uint height=power_to_2(h);

            return device->CreateColorRenderTarget(width,height,format);
        }
        
        bool ThemeEngine::Registry(Form *f,const VkFormat format)
        {
            if(!f)return(false);

            if(form_list.KeyExist(f))
                return(false);

            Vector2f size=f->GetSize();

            RenderTarget *rt=CreateRenderTarget(size.x,size.y,format);

            if(!rt)return(false);

            ThemeForm *tf=new ThemeForm(f,rt);

            form_list.Add(f,tf);

            return(true);
        }

        void ThemeEngine::Unregistry(Form *f)
        {
            if(!f)return;

            ThemeForm *tf;

            if(!form_list.Get(f,tf))
                return;

            delete tf;
        }

        bool ThemeEngine::Resize(Form *f,const uint32_t w,const uint32_t h,const VkFormat format)
        {
            if(!f)return(false);

            ThemeForm *tf;

            if(!form_list.Get(f,tf))return(false);

            if(w<=0||h<=0)
            {
                tf->SetRenderTarget(nullptr);
                return(true);
            }

            RenderTarget *old_rt=tf->GetRenderTarget();

            if(!old_rt)
            {
                const VkExtent2D old_size=old_rt->GetExtent();

                if(old_size.width>=w
                 &&old_size.height>=h)
                {
                    tf->Resize(w,h);
                    return(true);
                }
            }
                
            graph::RenderTarget *rt=CreateRenderTarget(w,h,format);

            if(!rt)return(false);

            tf->SetRenderTarget(rt);
            tf->Resize(w,h);
            return(true);
        }

        void ThemeEngine::Render(Form *f)
        {
            if(!f)return;

            
        }
    }//namespace gui
}//namespace hgl