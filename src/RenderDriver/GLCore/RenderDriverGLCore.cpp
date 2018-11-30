#include<hgl/graph/RenderDriver.h>

namespace hgl
{
    class RenderDriverGLCore:public RenderDriver
    {
    public:

        void SetCurStatus(const RenderStatus &rs) override
        {
        }

        void ClearColorBuffer() override
        {
        }

        void ClearDepthBuffer() override
        {
        }
    };//class RenderDriverGLCore:public RenderDriver
}//namespace hgl
