#include<hgl/graph/shader/ShaderMaker.h>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::graph::shader;

SHADER_NAMESPACE_BEGIN
bool CreateDefaultMaterial();
SHADER_NAMESPACE_END

int main()
{
    CreateDefaultMaterial();

    return 0;
}
