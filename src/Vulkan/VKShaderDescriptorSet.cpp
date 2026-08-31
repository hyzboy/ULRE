#include<hgl/vk/VKShaderDescriptorSet.h>
#include<hgl/log/Log.h>
#include<unordered_set>
#include<vector>

namespace hgl::graph{
namespace
{
    // Phase 4（ShaderGen_Descriptor_ABI_Unification_Plan.md）：
    // binding 唯一来源是生成期显式传入的 preferred_binding
    //（数值真源 inc/hgl/common/DescriptorSetTypeDef.h 的绑定枚举）。
    // 原 TryGetFixedBinding 字符串名字表与 DynamicBindingStart 动态递增兜底
    // 已删除——固定 ABI 下不允许隐式分配，缺失即报错。
    void ReassignBindingsForSet(ShaderDescriptorSet &set_desc)
    {
        std::vector<ShaderDescriptor *> values;
        set_desc.descriptor_map.GetValueArray(values);

        std::unordered_set<int> used_bindings;
        for (auto *sd : values)
        {
            if (!sd)
                continue;

            sd->set_type = set_desc.set_type;
            sd->set = int(set_desc.set_type);
            sd->binding = -1;
        }

        for (auto *sd : values)
        {
            if (!sd || sd->preferred_binding < 0)
                continue;

            if (used_bindings.find(sd->preferred_binding) != used_bindings.end())
            {
                GLogError(u8"[ShaderDescriptorSet] duplicate preferred binding %d (set=%d, name='%s')",
                          sd->preferred_binding, int(set_desc.set_type), sd->name);
                continue;
            }

            sd->binding = sd->preferred_binding;
            used_bindings.insert(sd->preferred_binding);
        }

        for (auto *sd : values)
        {
            if (!sd || sd->binding >= 0)
                continue;

            GLogError(u8"[ShaderDescriptorSet] descriptor '%s' (set=%d) has no preferred binding — "
                      u8"fixed ABI requires an explicit binding from the binding enum",
                      sd->name, int(set_desc.set_type));
        }
    }
}

/**
* 添加一个描述符
*/
ShaderDescriptor *ShaderDescriptorSet::AddDescriptor(uint32_t shader_stage_flag_bits,ShaderDescriptor *new_sd)
{
    ShaderDescriptor *sd;

    if(descriptor_map.Get(new_sd->name,sd))
    {
        delete new_sd;
        sd->stage_flag|=shader_stage_flag_bits;
        return(sd);
    }
    else
    {
        new_sd->set_type=set_type;
        new_sd->set=int(set_type);
        new_sd->stage_flag=shader_stage_flag_bits;

        descriptor_map.Add(new_sd->name,new_sd);
        ReassignBindingsForSet(*this);

        count++;

        return(new_sd);
    }
}
}//namespace hgl::graph
