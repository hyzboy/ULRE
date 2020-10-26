#include<hgl/gui/ThemeForm.h>

namespace hgl
{
    namespace gui
    {
        ThemeForm::ThemeForm(Form *f,RenderTarget *rt)
        {
            form=f;
            render_target=rt;                
        }
        
        ThemeForm::~ThemeForm()
        {
        }

        bool ThemeForm::SetRenderTarget(RenderTarget *rt)
        {
            SAFE_CLEAR(render_target);

            if(!rt)
                return(true);

            render_target=rt;
            return(true);
        }

        void ThemeForm::Resize(uint w,uint h)
        {
            form->OnResize(w,h);
        }
    }//namespace gui
}//namespace hgl
