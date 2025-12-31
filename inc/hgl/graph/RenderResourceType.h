#pragma once

#include<hgl/graph/VK.h>

VK_NAMESPACE_BEGIN

enum class ResourceType
{
    Unknown=0,

    VkInstance,
    VkPhysicalDevice,
    VkDevice,

    VertexInputLayout,
    Material,
    MaterialInstance,

    FrameBuffer,

    Texture,
    Sampler,

    VertexBuffer,
    IndexBuffer,
    IndirectBuffer,
    UniformBuffer,
    StorageBuffer,

    Skeleton,           ///<骨骼信息
    SkeletonAnime,      ///<骨骼动画信息

    Primitive,          ///<原始的单个模型数据，由多个VBO+Index组成
    RawMesh,            ///<原始的静态模型数据，由一个Primitive和一个MaterialInstance组成
    StaticMesh,         ///<静态模型数据，由一个或多个RawMesh组成
    SkeletonMesh,       ///<骨骼模型数据，由一个或多个StaticMesh组成

    Font,

    Scene,
    Animation,
    Audio,
    Other,

    ENUM_CLASS_RANGE(Unknown,Other)
};

enum class ResourcePlace
{
    Unknown=0,

    Memory,         ///<内存
    Device,         ///<设备(如显存)
    Disk,           ///<硬盘
    LAN,            ///<局域网
    WAN,            ///<广域网
    Other,

    ENUM_CLASS_RANGE(Device,Other)
};

struct RenderResourceID
{
    union
    {
        struct 
        {
            uint device:1;                  ///<在设备
            uint memory:1;                  ///<在内存

            uint disk:1;                    ///<在硬盘
            uint network:1;                 ///<在网络
        };

        uint8 place;                        ///<数据所在位置
    };

    uint16 resource_type;                   ///<资源类型，对应ResourceType枚举

    uint16 thread_id;                       ///<线程ID

    uint32 id;                              ///<资源ID
};
VK_NAMESPACE_END
