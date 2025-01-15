#include<hgl/graph/RenderFramework.h>

VK_NAMESPACE_BEGIN

bool InitShaderCompiler();
void CloseShaderCompiler();

namespace 
{
    static int RENDER_FRAMEWORK_COUNT=0;

}//namespace

RenderFramework::RenderFramework(const OSString &an)
{
    app_name=an;


}

RenderFramework::~RenderFramework()
{
    --RENDER_FRAMEWORK_COUNT;

    if(RENDER_FRAMEWORK_COUNT==0)
    {
        CloseShaderCompiler();
    }
}

bool RenderFramework::Init(uint w,uint h)
{
    if(RENDER_FRAMEWORK_COUNT==0)
    {
        if(!InitShaderCompiler())
            return(false);

        logger::InitLogger(app_name);
    }

    ++RENDER_FRAMEWORK_COUNT;


}
VK_NAMESPACE_END
