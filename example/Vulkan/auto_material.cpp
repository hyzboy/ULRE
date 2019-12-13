#include<hgl/graph/shader/ShaderMaker.h>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::graph::shader;

BEGIN_SHADER_NAMESPACE
bool CreateDefaultMaterial();
END_SHADER_NAMESPACE

int main()
{
    CreateDefaultMaterial();

    return 0;
}
