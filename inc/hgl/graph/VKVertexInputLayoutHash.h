#pragma once
#include<hgl/graph/VKNamespace.h>
#include<hgl/CoreType.h>
VK_NAMESPACE_BEGIN
/**
 * 顶点输入流配置
 */
union VertexInputLayoutHash
{
    struct
    {
        bool Position       :1;
        bool Normal         :1;
        bool Tangnet        :1;
        bool Bitangent      :1;

        uint TexCoord       :4;
        uint Color          :4;

        bool Bone           :1;
        bool Assign         :1;
    };

    uint32 hash_code;
};

/**
* 计算指定配置下需要的流数量
*/
const uint GetVertexInputStreamCount(const VertexInputLayoutHash &);
VK_NAMESPACE_END
