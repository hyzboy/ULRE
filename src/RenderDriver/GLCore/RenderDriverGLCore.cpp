#include<hgl/graph/RenderDriver.h>

namespace hgl
{
    namespace graph
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
    }//namespace graph
}//namespace hgl
