#pragma once

#include <hgl/CoreType.h>
#include <hgl/type/EnumUtil.h>
#include <hgl/type/StrChar.h>

namespace hgl::graph
{
    enum class PrimitiveType:uint32
    {
        Points=0,
        Lines,
        LineStrip,
        Triangles,
        // Fan / TriangleStrip：仅 GLTFConvert 导入侧仍在产出（未来 glTF 重写时
        // 一并删除）——mesh 渲染路径不支持（mesh shader 输出恒 triangle list，
        // fan/strip 装配规则依赖连续顶点流，mesh 分组模型下跨组必然错，T3.5 已删
        // 生成侧模拟）。CPU 几何应显式转成 triangle list + IBO。
        TriangleStrip,
        Fan,

        ENUM_CLASS_RANGE(Points,Fan),

        Error
    };

    /// 名字表（单一真源）：TOML 资产与诊断日志共用，避免各处各写一套 switch。
    /// 名字与 GLSL/TOML 常见写法一致，大小写不敏感匹配。
    constexpr const char *PrimitiveTypeName[]=
    {
        "points",
        "lines",
        "line_strip",
        "triangles",
        "triangle_strip",
        "fan",
    };

    inline const char *GetPrimitiveTypeName(const PrimitiveType pt)
    {
        if(pt<PrimitiveType::BEGIN_RANGE||pt>PrimitiveType::END_RANGE)
            return "error";

        return PrimitiveTypeName[static_cast<int>(pt)];
    }

    inline PrimitiveType ParsePrimitiveType(const char *name,const int name_len=0)
    {
        if(!name||!*name)
            return PrimitiveType::Error;

        const int result=::hgl::find_str_in_array<char>(
            int(PrimitiveType::RANGE_SIZE),
            (const char **)PrimitiveTypeName,
            name,
            name_len);

        if(result<int(PrimitiveType::BEGIN_RANGE)||result>int(PrimitiveType::END_RANGE))
            return PrimitiveType::Error;

        return static_cast<PrimitiveType>(result);
    }
}//namespace hgl::graph
