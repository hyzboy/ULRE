#include"DefaultThemeForm.h"
#include<hgl/gui/Form.h>
#include<hgl/vk/VKCommandBuffer.h>

namespace hgl
{
    namespace gui
    {
        bool DefaultThemeForm::Render()
        {
            if(!cmd_buf->BeginRenderPass())
                return(false);



            cmd_buf->EndRenderPass();
            return(true);
        }
    }//namespace gui
}//namespace hgl
