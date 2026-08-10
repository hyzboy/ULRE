#pragma once

#include <hgl/shadergen/ShaderProgramKey.h>
#include <hgl/type/String.h>
#include <hgl/type/UnorderedMap.h>
#include <hgl/type/ValueArray.h>

namespace hgl::graph
{
    class ShaderProgram;

    class ShaderProgramLinkCache
    {
        UnorderedMap<AnsiString, ShaderProgram *> programs;

    public:
        ShaderProgram *Find(const mtl::ShaderProgramKey &key) const
        {
            return FindName(key.ToString());
        }

        ShaderProgram *FindName(const AnsiString &name) const
        {
            ShaderProgram *program = nullptr;
            programs.Get(name, program);
            return program;
        }

        bool Add(const mtl::ShaderProgramKey &key, ShaderProgram *program)
        {
            return AddName(key.ToString(), program);
        }

        bool AddName(const AnsiString &name, ShaderProgram *program)
        {
            if (!program || name.IsEmpty())
                return false;

            return programs.Add(name, program);
        }

        bool RemoveName(const AnsiString &name)
        {
            return programs.DeleteByKey(name);
        }

        bool Remove(const mtl::ShaderProgramKey &key)
        {
            return RemoveName(key.ToString());
        }

        void GetValues(ValueArray<ShaderProgram *> &out_values) const
        {
            programs.GetValueArray(out_values);
        }

        void Clear() { programs.Clear(); }
        int GetCount() const { return programs.GetCount(); }
    };
}
