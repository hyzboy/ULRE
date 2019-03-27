#include<hgl/graph/RenderState.h>

namespace hgl
{
    namespace graph
    {
        void ColorState::Apply()
        {
            glColorMask(red,green,blue,alpha);

            if(clear_color)
                glClearBufferfv(GL_COLOR,0,clear_color_value);
        }

        void DepthState::Apply()
        {
            glDepthMask((GLenum)depth_mask);
            glDepthFunc((GLenum)depth_func);
                
            if(depth_test)
                glEnable(GL_DEPTH_TEST);
            else
                glDisable(GL_DEPTH_TEST);
                
            if(clear_depth)                    
                glClearBufferfv(GL_DEPTH,0,&clear_depth_value);
        }
            
        void CullFaceState::Apply()
        {
            if(enabled)
            {
                glEnable(GL_CULL_FACE);
                glCullFace((GLenum)mode);
            }
            else
                glDisable(GL_CULL_FACE);
        }
            
        void PolygonModeState::Apply()
        {
            glPolygonMode((GLenum)face,
                          (GLenum)mode);
        }
    }//namespace graph
}//namespace hgl
