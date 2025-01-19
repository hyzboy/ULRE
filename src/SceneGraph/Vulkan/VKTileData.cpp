#include<hgl/graph/VKPhysicalDevice.h>
#include<hgl/graph/TileData.h>
#include<hgl/graph/module/TextureManager.h>

namespace
{
    using namespace hgl;

    void AnalyseSize(uint &fw,uint &fh,const uint w,const uint h,const uint count,const uint32_t max_texture_size)
    {
        uint total,tw,th,t;

        fw=fh=0;

        tw=max_texture_size;
        while(tw>=w)
        {
            th=max_texture_size;
            while(th>=h)
            {
                t=(tw/w)*(th/h);

                if(!fw)
                {
                    fw=tw;
                    fh=th;

                    total=t;
                }
                else
                {
                    if(t==count)
                    {
                        //正好，就要这么大的

                        fw=tw;
                        fh=th;

                        return;
                    }
                    else
                    if(t>count)                //要比要求的最大值大
                    {
                        if(t<total)            //找到最接近最大值的
                        {
                            //比现在选中的更节省
                            fw=tw;
                            fh=th;

                            total=t;
                        }
                    }
                }

                th>>=1;
            }

            tw>>=1;
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

    const uint32_t max_2d_texture=GetPhysicalDevice()->GetMaxImage2D();

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
