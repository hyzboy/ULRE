#pragma once

#include<hgl/type/Map.h>
#include<hgl/type/String.h>
#include<hgl/graph/VK.h>
#include<hgl/graph/VKDescriptorSetType.h>

VK_NAMESPACE_BEGIN

class DeviceBuffer;
class Texture;
class Material;
class MaterialParameters;

/**
 * 描述符绑定器<Br>
 * 一般用于注册通用数据，为材质进行自动绑定。
 */
class DescriptorBinding
{
    DescriptorSetType set_type;                     ///<描述符合集类型

    Map<AnsiString,DeviceBuffer *> ubo_map;
    Map<AnsiString,DeviceBuffer *> ssbo_map;
    Map<AnsiString,Texture *> texture_map;

public:

    const DescriptorSetType GetType()const{return set_type;}

public:

    DescriptorBinding(const DescriptorSetType &dst)
    {
        set_type=dst;
    }

    bool AddUBO(const AnsiString &name,DeviceBuffer *buf)
    {
        if(!buf)return(false);
        if(name.IsEmpty())return(false);

        return ubo_map.Add(name,buf);
    }

    DeviceBuffer *GetUBO(const AnsiString &name)
    {
        if(name.IsEmpty())return(nullptr);

        return GetObjectFromMap(ubo_map,name);
    }

    void RemoveUBO(DeviceBuffer *buf)
    {
        if(!buf)return;

        ubo_map.DeleteByValue(buf);
    }

    bool AddSSBO(const AnsiString &name,DeviceBuffer *buf)
    {
        if(!buf)return(false);
        if(name.IsEmpty())return(false);

        return ssbo_map.Add(name,buf);
    }

    DeviceBuffer *GetSSBO(const AnsiString &name)
    {
        if(name.IsEmpty())return(nullptr);

        return GetObjectFromMap(ssbo_map,name);
    }

    void RemoveSSBO(DeviceBuffer *buf)
    {
        if(!buf)return;

        ssbo_map.DeleteByValue(buf);
    }

    bool AddTexture(const AnsiString &name,Texture *tex)
    {
        if(!tex)return(false);
        if(name.IsEmpty())return(false);

        return texture_map.Add(name,tex);
    }

    Texture *GetTexture(const AnsiString &name)
    {
        if(name.IsEmpty())return(nullptr);

        return GetObjectFromMap(texture_map,name);
    }

    void RemoveTexture(Texture *tex)
    {
        if(!tex)return;

        texture_map.DeleteByValue(tex);
    }

private:

    void BindUBO(MaterialParameters *,const BindingMap &,bool dynamic);

public:

    bool Bind(Material *);
};//class DescriptorBinding

VK_NAMESPACE_END
