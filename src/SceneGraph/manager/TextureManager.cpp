#include<hgl/graph/manager/TextureManager.h>

VK_NAMESPACE_BEGIN

void TextureManager::Release(Texture *tex)
{
    if(!tex)
        return;

    texture_set.Delete(tex);
}

VK_NAMESPACE_END
