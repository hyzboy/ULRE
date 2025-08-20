#include<hgl/graph/VKPhysicalDevice.h>
#include<hgl/graph/TileData.h>
#include<hgl/graph/module/TextureManager.h>

namespace
{
    using namespace hgl;

    void AnalyseSize(uint &fw,uint &fh,const uint w,const uint h,const uint count,const uint32_t max_texture_size)
    {
        const uint mw=max_texture_size/w;
        const uint mh=max_texture_size/h;

        if(mw*mh<=count)        //最大值都不够，那就算了
        {
            fw=max_texture_size;
            fh=max_texture_size;

            return;
        }

        fw=fh=1;

        while(count>(fw/w)*(fh/h))
        {
            if(fw>=max_texture_size)
            {
                fw=max_texture_size;
            }

            if(fh>=max_texture_size)
            {
                fh=max_texture_size;
            }

            if(fw>fh)
            {
                fh<<=1;
            }
            else
            {
                fw<<=1;
            }
        }
    }//void AnalyseSize
}//namespace

VK_NAMESPACE_BEGIN
TileData *TextureManager::CreateTileData(const VkFormat format,const uint width,const uint height,const uint count)
{
    if(!CheckVulkanFormat(format))
        return(nullptr);

    if(width<=0||height<=0||count<=0)
        return(nullptr);

    const uint32_t max_2d_texture=GetPhyDevice()->GetMaxImage2D();

    uint tex_width,tex_height;

    AnalyseSize(tex_width,tex_height,width,height,count,max_2d_texture);

    const VulkanFormat *vf=GetVulkanFormat(format);

    if(!vf)return(nullptr);

    Texture2D *tex=nullptr;
    VkExtent2D extent={tex_width,tex_height};

    if(RangeCheck(vf->color))
    {
        tex=CreateTexture2D(new ColorTextureCreateInfo(format,extent));
    }
    else
    if(RangeCheck(vf->depth))
    {
        tex=CreateTexture2D(new DepthTextureCreateInfo(format,extent));
    }
    else
        return(nullptr);

    return(new TileData(this,tex,width,height));
}
VK_NAMESPACE_END
