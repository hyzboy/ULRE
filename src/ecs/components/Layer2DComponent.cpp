#include<hgl/ecs/components/Layer2DComponent.h>
#include<type_traits>

namespace hgl::ecs
{
    // 头文件自洽编译校验（无虚表 POD 行约束）
    static_assert(!std::is_polymorphic<Layer2DComponent>::value,
                  "Layer2DComponent 不应有虚表");
}//namespace hgl::ecs