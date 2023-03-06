#include<hgl/shadergen/ShaderCreaterVertex.h>

SHADERGEN_NAMESPACE_BEGIN

void ShaderCreaterVertex::UseDefaultMain()
{
    main_codes=R"(
void main()
{
    gl_Position=GetPosition();
})";
}
SHADERGEN_NAMESPACE_END