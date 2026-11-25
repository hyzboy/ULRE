#pragma once

#include<hgl/common/DescriptorSetTypeDef.h>

/**
 * 着色器缓冲绑定描述 —— 后端无关契约。
 *
 * Vulkan 层创建缓冲时只需要「这个缓冲属于哪个描述符集、叫什么名字」，
 * 不需要 SceneGraph 的 `ShaderBufferSources`（那里的 RootAddresses / MeshDrawParams
 * 才是上层布局）。因此把最小描述结构放在契约层，上层继续以同一个名字使用。
 */
namespace hgl::graph
{
    struct ShaderBufferDesc
    {
        const DescriptorSetType set_type;

        const char *name;
    };

    struct ShaderBufferSource:public ShaderBufferDesc
    {
        const char *struct_name;
    };
}//namespace hgl::graph
