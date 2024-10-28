#pragma once

#include<hgl/graph/VK.h>

VK_NAMESPACE_BEGIN

class GraphManager
{
    GPUDevice *device;

public:

            VkDevice            GetVkDevice         ();                                             ///<取得Vulkan设备句柄
            GPUDevice *         GetDevice           ()noexcept{return device;}                      ///<取得GPU设备指针
    const   GPUPhysicalDevice * GetPhysicalDevice   ()const;                                        ///<取得GPU物理设备指针


public:

    GraphManager(GPUDevice *dev)
    {
        device=dev;
    }
};//class GraphManager

VK_NAMESPACE_END
