#include<hgl/graph/shader/ShaderMaker.h>

BEGIN_SHADER_NAMESPACE
bool ShaderMaker::Make()
{
    if(!fin_node)
        return(false);

    if(!fin_node->CheckInputParam())
        return(false);

    return(true);
}
END_SHADER_NAMESPACE
