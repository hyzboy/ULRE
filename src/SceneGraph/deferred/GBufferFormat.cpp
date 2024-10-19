#include<hgl/graph/deferred/GBufferFormat.h>
#include<hgl/graph/VKDevice.h>

VK_NAMESPACE_BEGIN

const uint GBufferComponentConfig::GetComponentSize(const GBufferComponent &c)const noexcept
{
    uint result=0;

    for(uint i=0;i<count;i++)
        if(components[i]==c)
            return size[i];

    return(0);
}

const VkFormat GBufferFormat::GetFormatFromComponent(const GBufferComponent &c)const noexcept
{
    int index=components_index[size_t(c)];

    if(index<0||index>=GBUFFER_MAX_COMPONENTS)
        return(VK_FORMAT_UNDEFINED);

    return components_list[index].format;
}

const ColorMode GBufferFormat::GetColorMode()const noexcept
{
    {
        int index=components_index[size_t(GBufferComponent::RGB)];

        if(index>0)
            return ColorMode::RGB;
    }

    {
        int Y_index=components_index[size_t(GBufferComponent::Y)];

        if(Y_index>0)
        {
            int CbCr_index=components_index[size_t(GBufferComponent::CbCr)];

            if(CbCr_index>0)
                return ColorMode::YCbCr;

            return ColorMode::Luminance;
        }
    }

    return ColorMode::Unknow;
}

const uint GBufferFormat::GetNormalSize()const noexcept
{
    int index=components_index[size_t(GBufferComponent::Normal)];

    if(index<0||index>=GBUFFER_MAX_COMPONENTS)
        return(0);

    return components_list[index].GetComponentSize(GBufferComponent::Normal);
}

bool InitGBufferFormat(GPUDevice *device,GBufferFormat *bf)
{
    if(!device||!bf)
        return(false);

    
}

VK_NAMESPACE_END
