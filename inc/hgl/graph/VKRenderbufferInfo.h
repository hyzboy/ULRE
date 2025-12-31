#pragma once
#include<hgl/type/ArrayList.h>
#include<hgl/graph/VKFormat.h>

VK_NAMESPACE_BEGIN
class RenderbufferInfo
{
protected:

    ArrayList<VkFormat> color_format_list;
    VkFormat depth_stencil_format;

    bool _depth;
    bool _stencil;
    bool _depth_stencil;

    VkImageLayout color_final_image_layout;
    VkImageLayout depth_final_image_layout;

private:

    bool SetDepthOrStencil(const VkFormat format)
    {
        if(IsDepthFormat(format))
        {
            _depth=true;
            depth_stencil_format=format;

            if(IsStencilFormat(format))
            {
                _stencil=true;
                _depth_stencil=true;
            }
            
            return(true);
        }
        else
        if(IsStencilFormat(format))
        {
            _stencil=true;
            depth_stencil_format=format;
            return(true);
        }
        
        return(false);
    }

public:

    RenderbufferInfo()
    {
        ClearDepthStencil();

        color_final_image_layout=
        depth_final_image_layout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    RenderbufferInfo(const VkFormat format):RenderbufferInfo()
    {
        if(!CheckVulkanFormat(format))return;

        if(!SetDepthOrStencil(format))
            color_format_list.Add(format);
    }

    RenderbufferInfo(const VkFormat color,const VkFormat ds):RenderbufferInfo()
    {
        AddColor(color);        
        SetDepthOrStencil(ds);
    }

    RenderbufferInfo(const VkFormat *cl,const uint32_t count,const VkFormat ds):RenderbufferInfo()
    {
        for(uint32_t i=0;i<count;i++)
        {
            AddColor(*cl);
            ++cl;
        }

        SetDepthOrStencil(ds);
    }

    RenderbufferInfo(const ArrayList<VkFormat> &cl,const VkFormat ds):RenderbufferInfo(cl.GetData(),cl.GetCount(),ds){}

    bool AddColor(const VkFormat format)
    {
        if(!CheckVulkanFormat(format))return(false);

        if(IsDepthFormat(format)||IsStencilFormat(format))
            return(false);

        color_format_list.Add(format);
        return(true);
    }

    void ClearColor()
    {
        color_format_list.Free();
    }

    bool SetDepth(const VkFormat format)
    {
        if(!CheckVulkanFormat(format))return(false);

        if(!IsDepthFormat(format))return(false);

        depth_stencil_format=format;
        _depth=true;
        _stencil=_depth_stencil=IsStencilFormat(format);

        return(true);
    }

    bool SetStencil(const VkFormat format)
    {
        if(!CheckVulkanFormat(format))return(false);

        if(!IsStencilFormat(format))return(false);

        depth_stencil_format=format;
        _stencil=true;
        _depth=_depth_stencil=IsDepthFormat(format);

        return(true);
    }

    bool SetDepthStencil(const VkFormat format)
    {
        if(!IsDepthStencilFormat(format))return(false);

        depth_stencil_format=format;
        _depth=_stencil=_depth_stencil=true;
        return(true);
    }

    void ClearDepthStencil()
    {        
        depth_stencil_format=PF_UNDEFINED;
        _depth=_stencil=_depth_stencil=false;
    }

    void SetColorImageLayout(const VkImageLayout il)
    {
        color_final_image_layout=il;
    }

    void SetDepthImageLayout(const VkImageLayout il)
    {
        depth_final_image_layout=il;
    }

public:

    const uint32_t  GetAttachmentCount()const
    {
        return(depth_stencil_format==PF_UNDEFINED
                ?color_format_list.GetCount()
                :color_format_list.GetCount()+1);
    }

            const bool      HasDepth()const{return _depth;}
            const bool      HasStencil()const{return _stencil;}
            const bool      HasDepthStencil()const{return _depth_stencil;}
            const bool      HasDepthOrStencil()const{return(_depth||_stencil);}
    virtual const bool      IsSwapchain()const{return false;}

    const uint32_t          GetColorCount ()const{return color_format_list.GetCount();}
    const VkFormat *        GetColorFormat()const{return color_format_list.GetData();}
    const VkFormat          GetDepthFormat()const{return depth_stencil_format;}
    const ArrayList<VkFormat> &  GetColorFormatList()const{return color_format_list;}

    const VkImageLayout     GetColorLayout()const{return color_final_image_layout;}
    const VkImageLayout     GetDepthLayout()const{return depth_final_image_layout;}
};//class RenderbufferFormatInfo

class SwapchainRenderbufferInfo:public RenderbufferInfo
{
public:

    SwapchainRenderbufferInfo(const VkFormat cf,const VkFormat df):RenderbufferInfo(cf,df)
    {
        this->color_final_image_layout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        this->depth_final_image_layout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    const bool      IsSwapchain()const override{return true;}
};//

class FramebufferInfo:public RenderbufferInfo
{
    VkExtent2D extent;

public:

    using RenderbufferInfo::RenderbufferInfo;

    FramebufferInfo(const VkFormat format,const VkExtent2D &ext):RenderbufferInfo(format)
    {
        extent=ext;
    }

    FramebufferInfo(const VkFormat format,const uint32_t w,const uint32_t h):RenderbufferInfo(format)
    {
        SetExtent(w,h);
    }
    
    void SetExtent(const VkExtent2D &ext)
    {
        extent=ext;
    }

    void SetExtent(const uint32_t w,const uint32_t h)
    {
        extent.width=w;
        extent.height=h;
    }

    const VkExtent2D &GetExtent()const{return extent;}

    const uint32_t GetWidth()const{return extent.width;}
    const uint32_t GetHeight()const{return extent.height;}
};//class FramebufferInfo:public RenderbufferFormatInfo

using FBOInfo=FramebufferInfo;
VK_NAMESPACE_END
