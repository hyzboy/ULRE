#include<hgl/graph/manager/TextureManager.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKCommandBuffer.h>

VK_NAMESPACE_BEGIN

TextureManager::TextureManager(GPUDevice *dev):GraphManager(dev)
{
    texture_cmd_buf=dev->CreateTextureCommandBuffer(GetPhysicalDevice()->GetDeviceName()+AnsiString(":TexCmdBuffer"));
    texture_queue=dev->CreateQueue();

    texture_serial=0;
}

TextureManager::~TextureManager()
{
    SAFE_CLEAR(texture_queue);
    SAFE_CLEAR(texture_cmd_buf);
}

const TextureID TextureManager::Add(Texture *tex)
{
    if(!tex)
        return(-1);

    if(tex->GetManager()!=this)
        return(-2);

    if(texture_set.Contains(tex))
        return tex->GetID();

    texture_set.Add(tex);
    texture_by_id.Add(tex->GetID(),tex);

    return tex->GetID();
}

const TextureID TextureManager::Add(Texture *tex,const OSString &tn)
{
    TextureID id=Add(tex);

    if(id<0)
        return id;

    if(!tn.IsEmpty())
        texture_by_filename.Add(tn,tex);

    return id;
}

void TextureManager::Release(Texture *tex)
{
    if(!tex)
        return;

    if(!texture_set.Contains(tex))
        return;

    texture_set.Delete(tex);
    texture_by_id.DeleteByKey(tex->GetID());
    texture_by_filename.DeleteByValue(tex);
}

Texture2D *CreateTexture2DFromFile(TextureManager *tm,const OSString &filename,bool auto_mipmaps);

Texture2D *TextureManager::LoadTexture2D(const OSString &filename,bool auto_mipmaps)
{
    Texture2D *tex;
    
    if(texture_by_filename.Get(filename,(Texture *&)tex))
        return tex;
    
    tex=CreateTexture2DFromFile(this,filename,auto_mipmaps);

    if(tex)
    {
        Add(tex,filename);

    //#ifdef _DEBUG
    //    DebugUtils *du=device->GetDebugUtils();

    //    if(du)
    //    {
    //        const UTF8String name=U8_TEXT("Tex2D:")+ToUTF8String(filename);
    //    
    //        du->SetImage(tex->GetImage(),(char *)(name.c_str()));
    //    }
    //#endif//_DEBUG
    }

    return tex;
}

Texture2DArray *TextureManager::CreateTexture2DArray(const AnsiString &name,const uint32_t width,const uint32_t height,const uint32_t layer,const VkFormat &fmt,bool auto_mipmaps)
{
    Texture2DArray *ta=CreateTexture2DArray(width,height,layer,fmt,auto_mipmaps);

    if(ta)
        Add(ta);
    else
        return nullptr;

    //#ifdef _DEBUG
    //    DebugUtils *du=device->GetDebugUtils();
    //    
    //    if(du)
    //    {
    //        du->SetImage(ta->GetImage(),"Tex2DArrayImage:"+name);
    //        du->SetImageView(ta->GetVulkanImageView(),"Tex2DArrayImageView:"+name);
    //        du->SetDeviceMemory(ta->GetDeviceMemory(),"Tex2DArrayMemory:"+name);
    //    }
    //#endif//_DEBUG

    return ta;
}

bool LoadTexture2DLayerFromFile(TextureManager *tm,Texture2DArray *t2d,const uint32_t layer,const OSString &filename,bool auto_mipmaps);

bool TextureManager::LoadTexture2DToArray(Texture2DArray *ta,const uint32_t layer,const OSString &filename)
{
    if(!ta)return(false);

    if(!LoadTexture2DLayerFromFile(this,ta,layer,filename,false))
        return(false);

    return(true);
}

TextureCube *CreateTextureCubeFromFile(TextureManager *tm,const OSString &filename,bool auto_mipmaps);

TextureCube *TextureManager::LoadTextureCube(const OSString &filename,bool auto_mipmaps)
{
    TextureCube *tex;

    if(texture_by_filename.Get(filename,(Texture *&)tex))
        return tex;

    tex=CreateTextureCubeFromFile(this,filename,auto_mipmaps);

    if(tex)
    {
        Add(tex,filename);
    }

    return tex;
}

VK_NAMESPACE_END
