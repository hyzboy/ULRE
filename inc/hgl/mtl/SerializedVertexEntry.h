#pragma once

// SerializedVertexEntry —— 仅 ShaderGen 生成侧使用的顶点条目中间表示。
// 运行时已不消费（顶点数据统一走 Vertex 集 SSBO，传统 VAB/VBO 路径已删除）；
// 使用方仅剩 ShaderGen 编译链与 ShaderResourceSchemaRegressionGate 工具。

#include<vulkan/vulkan.h>
#include<hgl/common/VertexAttribDef.h>

namespace hgl::graph::mtl{

struct SerializedVertexEntry
{
    VkFormat        format;
    VertexSemantic  semantic;

    bool operator==(const SerializedVertexEntry &rhs) const noexcept
    {
        return format == rhs.format && semantic == rhs.semantic;
    }
};

}//namespace hgl::graph::mtl
