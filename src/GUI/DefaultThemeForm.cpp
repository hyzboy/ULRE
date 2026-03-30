#include"DefaultThemeForm.h"
#include<hgl/gui/Form.h>
#include<hgl/vk/VKCommandBuffer.h>

namespace hgl
{
    namespace gui
    {
        bool DefaultThemeForm::Render()
        {
            RenderTargetData *rtd=render_target->GetCurrentRTD();

            cmd_buf->EndRenderingDynamic(rtd);
            return(true);
        }
    }//namespace gui
}//namespace hgl
