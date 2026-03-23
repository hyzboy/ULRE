#pragma once

#include<hgl/CoreType.h>

namespace hgl::graph::mtl
{
    enum class DescriptorKind : uint8
    {
        UBO,
        SSBO,
        Texture,
        TextureSampler,
    };

    enum class DescriptorSemantic : uint8
    {
        Unknown = 0,

        ViewportInfo,
        CameraInfo,
        SkyInfo,

        TransformID,
        LocalToWorld,
        MaterialInstanceID,
        MaterialInstance,

        MaterialInstanceTextureID,        ///<当使用TextureArray时的材质ID信息
        //比如一个材质有BaseColor,Normal两个纹理，但是开发者配置了使用TextureArray
        //那么原本的纹理的Sampler2D TexBaseColor;就会变成Sampler2DArray TexBaseColor;
        //然后 glsl 中会出现名为 MaterialInstanceTexture 的结构，结构里面有 uint BaseColor; uint Normal;这样的成员，表示当前材质实例使用的纹理ID（TextureArray的Layer index）。如果后续有其它类型的纹理，也会自动扩展这个结构体，增加成员。
        //再之后会出现名为 MaterialInstanceTextureID 的SSBO, 这的结构内部是MaterialInstanceTexture tex_id[];
        // 后面要访问纹理内容的，全部根据材质ID，从MaterialInstanceTextureID SSBO里取出对应的纹理ID，再去访问TextureArray。这样就实现了在不增加DrawCall的前提下，单个材质实例使用不同纹理的功能。

        //MaterialSampler,  //未启用

        ColorPattle,        ///<调色板(目前仅Line绘制使用)

        BoneJoint,          ///<骨骼节点ID
        BoneJointWeight,    ///<骨骼权重

        Custom,
    };

    enum class UBODescriptorSemantic : uint8
    {
        Unknown = 0,
        ViewportInfo,
        CameraInfo,
        SkyInfo,
        ColorPattle,
        Custom,
    };

    enum class SSBODescriptorSemantic : uint8
    {
        Unknown = 0,
        TransformID,
        LocalToWorld,
        MaterialInstanceID,
        MaterialInstance,
        MaterialInstanceTextureID,
        BoneJoint,
        BoneJointWeight,
        Custom,
    };
}//namespace hgl::graph::mtl
