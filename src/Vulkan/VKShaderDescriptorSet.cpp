#include<hgl/vk/VKShaderDescriptorSet.h>
#include<algorithm>
#include<cstring>
#include<unordered_set>
#include<vector>

namespace hgl::graph{
namespace
{
    bool TryGetFixedBinding(DescriptorSetType set_type, const char *name, int &out_binding)
    {
        if (!name || !*name)
            return false;

        switch (set_type)
        {
        case DescriptorSetType::Scene:
            if (strcmp(name, "camera") == 0)   { out_binding = 0; return true; }
            if (strcmp(name, "sky") == 0)      { out_binding = 1; return true; }
            if (strcmp(name, "viewport") == 0) { out_binding = 2; return true; }
            return false;

        case DescriptorSetType::Transform:
            if (strcmp(name, "l2w") == 0)            { out_binding = 0; return true; }
            if (strcmp(name, "l2w_index_rows") == 0) { out_binding = 1; return true; }
            return false;

        case DescriptorSetType::VertexData:
            if (strcmp(name, "VertexDataBuffer") == 0 || strcmp(name, "vtx_data") == 0)
            {
                out_binding = 18;
                return true;
            }
            if (strcmp(name, "IndexDataBuffer") == 0 || strcmp(name, "idx_data") == 0)
            {
                out_binding = 19;
                return true;
            }
            return false;

        case DescriptorSetType::Material:
            return false;

        default:
            return false;
        }
    }

    int DynamicBindingStart(DescriptorSetType set_type)
    {
        switch (set_type)
        {
        case DescriptorSetType::Scene:     return 3;
        case DescriptorSetType::Transform: return 2;
        case DescriptorSetType::Material:  return 0;
        case DescriptorSetType::VertexData:return 20;
        default:                           return 0;
        }
    }

    void ReassignBindingsForSet(ShaderDescriptorSet &set_desc)
    {
        std::vector<ShaderDescriptor *> values;
        set_desc.descriptor_map.GetValueArray(values);

        std::sort(values.begin(), values.end(), [](const ShaderDescriptor *a, const ShaderDescriptor *b)
        {
            const char *lhs = (a && a->name) ? a->name : "";
            const char *rhs = (b && b->name) ? b->name : "";
            return std::strcmp(lhs, rhs) < 0;
        });

        std::unordered_set<int> used_bindings;
        for (auto *sd : values)
        {
            if (!sd)
                continue;

            sd->set_type = set_desc.set_type;
            sd->set = int(set_desc.set_type);
            sd->binding = -1;
        }

        // 1) Explicit preferred binding from compiler contract (highest priority).
        for (auto *sd : values)
        {
            if (!sd || sd->preferred_binding < 0)
                continue;

            sd->binding = sd->preferred_binding;
            used_bindings.insert(sd->preferred_binding);
        }

        // 2) Legacy fixed binding by well-known descriptor names.
        for (auto *sd : values)
        {
            if (!sd || sd->binding >= 0)
                continue;

            int fixed_binding = -1;
            if (TryGetFixedBinding(set_desc.set_type, sd->name, fixed_binding))
            {
                sd->binding = fixed_binding;
                used_bindings.insert(fixed_binding);
            }
        }

        int candidate = DynamicBindingStart(set_desc.set_type);
        for (auto *sd : values)
        {
            if (!sd)
                continue;

            int fixed_binding = -1;
            if (TryGetFixedBinding(set_desc.set_type, sd->name, fixed_binding))
                continue;

            while (used_bindings.find(candidate) != used_bindings.end())
                ++candidate;

            sd->binding = candidate;
            used_bindings.insert(candidate);
            ++candidate;
        }
    }
}

/**
* 添加一个描述符
*/
ShaderDescriptor *ShaderDescriptorSet::AddDescriptor(uint32_t ssb,ShaderDescriptor *new_sd)
{
    ShaderDescriptor *sd;

    if(descriptor_map.Get(new_sd->name,sd))
    {
        delete new_sd;
        sd->stage_flag|=ssb;
        return(sd);
    }
    else
    {
        new_sd->set_type=set_type;
        new_sd->set=int(set_type);
        new_sd->stage_flag=ssb;

        descriptor_map.Add(new_sd->name,new_sd);
        ReassignBindingsForSet(*this);

        count++;

        return(new_sd);
    }
}
}//namespace hgl::graph
