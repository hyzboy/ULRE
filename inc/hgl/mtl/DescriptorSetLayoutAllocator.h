#pragma once

namespace hgl::graph::mtl {}

#include <hgl/common/ShaderDescriptorDef.h>
#include<hgl/graph/ShaderBufferSource.h>
#include<ankerl/unordered_dense.h>
#include<string>

namespace hgl{namespace graph{namespace mtl{
    using namespace hgl::graph::mtl;
/**
* 材质描述符管理</p>
* 该类使用于SHADER生成前，用于统计编号set/binding
*/
class DescriptorSetLayoutAllocator
{
    ShaderDescriptorSetArray desc_set_array;

    ankerl::unordered_dense::map<std::string,std::string> struct_map;
    ankerl::unordered_dense::map<std::string,UBODescriptor *> ubo_map;
    ankerl::unordered_dense::map<std::string,SSBODescriptor *> ssbo_map;

private:

    static std::string KeyFrom(const std::string &text)
    {
        return text;
    }

    static std::string KeyFrom(const char *text)
    {
        return std::string(text?text:"");
    }

public:

    DescriptorSetLayoutAllocator();
    ~DescriptorSetLayoutAllocator()=default;

    bool AddStruct(const std::string &name,const std::string &code)
    {
        struct_map[KeyFrom(name)] = KeyFrom(code);
        return(true);
    }
    bool AddStruct(const char *name,const char *code)
    {
        return AddStruct(KeyFrom(name),KeyFrom(code));
    }
    bool AddStruct(const char *name,const std::string &code)
    {
        return AddStruct(KeyFrom(name),code);
    }
    bool AddStruct(const std::string &name,const char *code)
    {
        return AddStruct(name,KeyFrom(code));
    }

    bool hasStruct(const std::string &name) const
    {
        return struct_map.contains(name);
    }

    const UBODescriptor *AddUBO(uint32_t shader_stage_flag_bits,DescriptorSetType set_type,UBODescriptor *sd);
    const SSBODescriptor *AddSSBO(uint32_t shader_stage_flag_bits,DescriptorSetType set_type,SSBODescriptor *sd);

    UBODescriptor *GetUBO(const std::string &name);
    SSBODescriptor *GetSSBO(const std::string &name);
    const UBODescriptor *GetUBO(const std::string &name)const{return const_cast<DescriptorSetLayoutAllocator *>(this)->GetUBO(name);}
    const SSBODescriptor *GetSSBO(const std::string &name)const{return const_cast<DescriptorSetLayoutAllocator *>(this)->GetSSBO(name);}
    UBODescriptor *GetUBO(const char *name){return GetUBO(std::string(name?name:""));}
    SSBODescriptor *GetSSBO(const char *name){return GetSSBO(std::string(name?name:""));}
    const UBODescriptor *GetUBO(const char *name)const{return GetUBO(std::string(name?name:""));}
    const SSBODescriptor *GetSSBO(const char *name)const{return GetSSBO(std::string(name?name:""));}

    const uint GetCount()const
    {
        uint count = 0;

        for (const auto &p : desc_set_array)
            if (p.count > 0)
                count += static_cast<uint>(p.count);

        return count;
    }

    const ShaderDescriptorSetArray &Get()const
    {
        return desc_set_array;
    }

    const bool hasSet(const DescriptorSetType &type)const
    {
        return desc_set_array[size_t(type)].count>0;
    }
};//class DescriptorSetLayoutAllocator
}}}//namespace hgl::graph::mtl
