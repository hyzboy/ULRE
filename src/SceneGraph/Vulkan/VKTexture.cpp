#include<hgl/graph/VKTexture.h>
#include<hgl/graph/VKImageView.h>
#include<hgl/graph/VKMemory.h>
#include<hgl/graph/module/TextureManager.h>
VK_NAMESPACE_BEGIN
Texture::~Texture()
{
    if(!data)return;

    if(data->image_view)
        delete data->image_view;

    if(data->memory)        //没有memory的纹理都是从其它地方借来的，所以就不存在删除
    {
        delete data->memory;

        if(data->image)
            vkDestroyImage(manager->GetVkDevice(),data->image,nullptr);
    }
}
VK_NAMESPACE_END
