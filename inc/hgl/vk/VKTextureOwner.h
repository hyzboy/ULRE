#pragma once

#include<hgl/vk/VK.h>

namespace hgl::graph
{
    class Texture;

    /**
     * 纹理的**所有者契约**（下层需要、上层实现）。
     *
     * 为什么需要它：`Texture` 的析构与 bindless 视图创建里有一部分行为属于**上层管理**
     * （通知 bindless 经理纹理已销毁、从所有者表里注销），另有一部分只依赖设备
     * （`VkDevice`、销毁 image/view）。把这些行为放在 Vulkan 层会迫使 Vulkan 反向
     * include 上层的 `TextureManager` / `GraphicsContext`。
     *
     * 本接口只暴露下层真正需要的那几件事，由 `TextureManager` 实现 ⇒
     * `VKTexture.cpp` 不再认识任何上层类型。
     */
    class TextureOwner
    {
    public:
        virtual ~TextureOwner() = default;

        /// 下层创建伴生 image view 时需要（与 `GraphModule::GetVkDevice()` 同值）。
        /// 刻意**不叫** `GetVkDevice` —— 实现方（`TextureManager`）已从模块基类继承同名
        /// 成员，同名会让 `this->GetVkDevice()` 产生重载歧义。
        virtual VkDevice GetOwnerVkDevice() const = 0;

        /**
         * 纹理**即将**析构（在 image/view 被销毁之前调用）。
         * 实现方在此完成两件上层动作：通知 bindless 经理该 unique_id 已失效、
         * 把该纹理从自己的登记表（纹理集 / ID 映射 / 文件名映射）里注销。
         */
        virtual void OnTextureDestroying(Texture *texture) = 0;
    };
}//namespace hgl::graph