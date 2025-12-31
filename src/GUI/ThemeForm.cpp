#include<hgl/gui/ThemeForm.h>
#include<hgl/graph/VKCommandBuffer.h>

namespace hgl
{
    namespace gui
    {
        ThemeForm::ThemeForm(Form *f,RenderTarget *rt,RenderCmdBuffer *rc)
        {
            form=f;
            render_target=rt;
            cmd_buf=rc;
        }
        
        ThemeForm::~ThemeForm()
        {
            delete cmd_buf;
            delete render_target;
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
        
        bool ThemeForm::BeginRender()
        {
            if(!cmd_buf->Begin())return(false);
            if(!cmd_buf->BindRenderTarget(render_target->GetRenderPass(),render_target->GetFramebuffer()))return(false);

            cmd_buf->SetClearColor(0,0,0,0,1.0f);

            return(true);
        }
        
        bool ThemeForm::EndRender()
        {
            if(!cmd_buf->End())return(false);

            if(!render_target->Submit(cmd_buf))return(false);
            if(!render_target->WaitQueue())return(false);

            return(true);
        }
    }//namespace gui
}//namespace hgl
