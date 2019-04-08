#include"VKInstance.h"

int main(int,char **)
{
    using namespace hgl;
    using namespace hgl::graph;

    vulkan::Instance inst("Test");

    inst.Init();

    return 0;
}
