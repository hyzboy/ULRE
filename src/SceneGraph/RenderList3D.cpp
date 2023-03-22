#include<hgl/graph/RenderList.h>

namespace hgl
{
    namespace graph
    {
        RenderList3D::RenderList3D()
        {   
            hgl_zero(camera_info);

            mvp_array       =new GPUArrayBuffer(device,VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,MVPMatrixBytes);
        }

        RenderList3D::~RenderList3D()
        {
            delete mvp_array;
        }
        
        bool RenderList3D::Begin()
        {
            if(!RenderList::Begin())
                return(false);

            mvp_array->Clear();

            return(true);
        }

        bool RenderList3D::Expend(SceneNode *)
        {
        }

        void RenderList3D::End()
        {
        }
    }//namespace graph
}//namespace hgl
