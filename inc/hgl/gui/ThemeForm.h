#ifndef HGL_GUI_THEME_FORM_INCLUDE
#define HGL_GUI_THEME_FORM_INCLUDE

#include<hgl/graph/VKRenderTarget.h>
#include<hgl/gui/Form.h>

namespace hgl
{
    namespace gui
    {
        using namespace hgl::graph;

        class ThemeForm
        {
        protected:
        
            Form *form;

            RenderTarget *render_target;
            RenderCommand *cmd_buf;

        public:

            ThemeForm(Form *,RenderTarget *,RenderCommand *);
            virtual ~ThemeForm();

                    RenderTarget *  GetRenderTarget(){return render_target;}
                    bool            SetRenderTarget(RenderTarget *);

                    void            Resize(uint w,uint h);

                    bool            BeginRender();
            virtual bool            Render()=0;
                    bool            EndRender();
        };//class ThemeForm
    }//namespace gui
}//namespace hgl
#endif//HGL_GUI_THEME_FORM_INCLUDE
