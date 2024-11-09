#include<hgl/graph/font/TileFont.h>
#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/module/SwapchainModule.h>


VK_NAMESPACE_BEGIN
/**
 * 创建只使用一种字符的Tile字符管理对象
 * @param f 字体需求信息
 * @param limit_count 缓冲字符数量上限
 */
TileFont *RenderFramework::CreateTileFont(FontSource *fs,int limit_count)
{
    if(!fs)return(nullptr);

    const uint32_t height=hgl_align_pow2(fs->GetCharHeight()+2,4);  //上下左右各空一个象素，并保证可以被4整除

    if(limit_count<=0)
    {
        const VkExtent2D &ext=swapchain_module->GetSwapchainSize();

        limit_count=(ext.width/height)*(ext.height/height);   //按全屏幕放满不一样的字符为上限
    }

    if(!fs)
        return(nullptr);

    TileData *td=CreateTileData(UPF_R8,height,height,limit_count);

    if(!td)
        return nullptr;

    return(new TileFont(td,fs));
}
VK_NAMESPACE_END
