#pragma once

#include<hgl/type/UnorderedMap.h>
#include<hgl/type/String.h>
#include<hgl/graph/VKBuffer.h>

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

    UnorderedMap<AnsiString,DeviceBuffer *> ubo_map;
    UnorderedMap<AnsiString,DeviceBuffer *> ssbo_map;
    UnorderedMap<AnsiString,Texture *> texture_map;

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

    template<typename T>
    bool AddUBO(const AnsiString &name,DeviceBufferMap<T> *dbm)
    {
        if(name.IsEmpty()||!dbm)
            return(false);

        return ubo_map.Add(name,dbm->GetDeviceBuffer());
    }

    template<typename T>
    bool AddUBO(const UBOInstance<T> *ubo_instance)
    {
        if(!ubo_instance)
            return(false);

        if(ubo_instance->set_type()!=set_type)
            return(false);

        if(ubo_instance->name().IsEmpty())
            return(false);

        ubo_instance->Update();

        return ubo_map.Add(ubo_instance->name(),ubo_instance->ubo());
    }

    DeviceBuffer *GetUBO(const AnsiString &name)
    {
        if(name.IsEmpty())return(nullptr);

        DeviceBuffer** ptr = ubo_map.GetValuePointer(name);
        return ptr ? *ptr : nullptr;
    }

    void RemoveUBO(const AnsiString &name)
    {
        if(name.IsEmpty())return;

        ubo_map.DeleteByKey(name);
    }

    bool AddSSBO(const AnsiString &name,DeviceBuffer *buf)
    {
        if(!buf)return(false);
        if(name.IsEmpty())return(false);

        return ssbo_map.Add(name,buf);
    }

    template<typename T>
    bool AddSSBO(const AnsiString &name,DeviceBufferMap<T> *dbm)
    {
        return AddSSBO(name,dbm->GetDeviceBuffer());
    }

    DeviceBuffer *GetSSBO(const AnsiString &name)
    {
        if(name.IsEmpty())return(nullptr);

        DeviceBuffer** ptr = ssbo_map.GetValuePointer(name);
        return ptr ? *ptr : nullptr;
    }

    void RemoveSSBO(const AnsiString &name)
    {
        if(name.IsEmpty())return;

        ssbo_map.DeleteByKey(name);
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

        Texture** ptr = texture_map.GetValuePointer(name);
        return ptr ? *ptr : nullptr;
    }

    void RemoveTexture(const AnsiString &name)
    {
        if(name.IsEmpty())return;

        texture_map.DeleteByKey(name);
    }

private:

    void BindUBO(MaterialParameters *,const BindingMap &,bool dynamic);

public:

    bool Bind(Material *);
};//class DescriptorBinding

VK_NAMESPACE_END
