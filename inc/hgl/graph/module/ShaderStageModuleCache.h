#pragma once

#include <hgl/common/ShaderStageDef.h>
#include <hgl/mtl/ShaderStageKey.h>
#include <hgl/type/String.h>
#include <hgl/type/UnorderedMap.h>
#include <hgl/type/ValueArray.h>
#include <hgl/vk/VKShaderModule.h>

namespace hgl::graph
{
    class ShaderStageModuleCache
    {
        UnorderedMap<AnsiString, ShaderModule *> modules;

        static AnsiString MakeNameKey(const ShaderStage stage, const AnsiString &name)
        {
            return AnsiString::numberOf(static_cast<uint32>(stage)) + AnsiString(":") + name;
        }

    public:
        ShaderModule *Find(const mtl::ShaderStageKey &key) const
        {
            return FindName(key.stage, key.ToString());
        }

        ShaderModule *FindName(const ShaderStage stage, const AnsiString &name) const
        {
            ShaderModule *module = nullptr;
            modules.Get(MakeNameKey(stage, name), module);
            return module;
        }

        bool Add(const mtl::ShaderStageKey &key, ShaderModule *module)
        {
            return AddName(key.stage, key.ToString(), module);
        }

        bool AddName(const ShaderStage stage, const AnsiString &name, ShaderModule *module)
        {
            if (!module || name.IsEmpty())
                return false;

            return modules.Add(MakeNameKey(stage, name), module);
        }

        bool RemoveName(const ShaderStage stage, const AnsiString &name)
        {
            return modules.DeleteByKey(MakeNameKey(stage, name));
        }

        void GetValues(ValueArray<ShaderModule *> &out_values) const
        {
            modules.GetValueArray(out_values);
        }

        void Clear() { modules.Clear(); }
        int GetCount() const { return modules.GetCount(); }
    };
}
