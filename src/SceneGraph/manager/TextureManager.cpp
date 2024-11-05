#include<hgl/graph/manager/TextureManager.h>

VK_NAMESPACE_BEGIN

void TextureManager::Release(Texture *tex)
{
    if(!tex)
        return;

    texture_set.Delete(tex);
}

Texture2D *CreateTexture2DFromFile(GPUDevice *device,const OSString &filename,bool auto_mipmaps);

Texture2D *TextureManager::LoadTexture2D(const OSString &filename,bool auto_mipmaps)
{
    Texture2D *tex;
    
    if(texture_by_name.Get(filename,(Texture *&)tex))
        return tex;
    
    tex=CreateTexture2DFromFile(device,filename,auto_mipmaps);

    if(tex)
    {
        texture_by_name.Add(filename,tex);
        Add(tex);

    #ifdef _DEBUG
        DebugUtils *du=device->GetDebugUtils();

        if(du)
        {
            const UTF8String name=U8_TEXT("Tex2D:")+ToUTF8String(filename);
        
            du->SetImage(tex->GetImage(),(char *)(name.c_str()));
        }
    #endif//_DEBUG
    }

    return tex;
}

Texture2DArray *TextureManager::CreateTexture2DArray(const AnsiString &name,const uint32_t width,const uint32_t height,const uint32_t layer,const VkFormat &fmt,bool auto_mipmaps)
{
    Texture2DArray *ta=device->CreateTexture2DArray(width,height,layer,fmt,auto_mipmaps);

    if(ta)
        Add(ta);
    else
        return nullptr;

    #ifdef _DEBUG
        DebugUtils *du=device->GetDebugUtils();
        
        if(du)
        {
            du->SetImage(ta->GetImage(),"Tex2DArrayImage:"+name);
            du->SetImageView(ta->GetVulkanImageView(),"Tex2DArrayImageView:"+name);
            du->SetDeviceMemory(ta->GetDeviceMemory(),"Tex2DArrayMemory:"+name);
        }
    #endif//_DEBUG

    return ta;
}

bool LoadTexture2DLayerFromFile(TextureManager *tm,Texture2DArray *t2d,const uint32_t layer,const OSString &filename,bool auto_mipmaps);

bool TextureManager::LoadTexture2DToArray(Texture2DArray *ta,const uint32_t layer,const OSString &filename)
{
    if(!ta)return(false);

    if(!LoadTexture2DLayerFromFile(device,ta,layer,filename,false))
        return(false);

    return(true);
}

TextureCube *CreateTextureCubeFromFile(GPUDevice *device,const OSString &filename,bool auto_mipmaps);

TextureCube *TextureManager::LoadTextureCube(const OSString &filename,bool auto_mipmaps)
{
    TextureCube *tex;

    if(texture_by_name.Get(filename,(Texture *&)tex))
        return tex;

    tex=CreateTextureCubeFromFile(device,filename,auto_mipmaps);

    if(tex)
    {
        texture_by_name.Add(filename,tex);
        Add(tex);
    }

    return tex;
}

VK_NAMESPACE_END
