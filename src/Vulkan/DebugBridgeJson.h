#pragma once

#include<hgl/type/String.h>
#include<hgl/util/json/JsonTool.h>

namespace hgl::graph::debug_bridge
{
    /**
     * Debug Bridge 内部 JSON 文本互转辅助。
     *
     * 桥内 RPC 结果统一以 nlohmann::json 结构化组装，仅在写入管道前
     * 序列化一次（DebugJsonToText）；尚未重构的 AnsiString 版 GetStatusJSON
     * 生产者经 DebugJsonFromText 接入，避免在信封中手工拼接 JSON 文本。
     */

    // json → AnsiString（UTF-8 字节）；序列化失败返回空串
    inline AnsiString DebugJsonToText(const nlohmann::json &value)
    {
        U8String text;
        OSString error;
        if(!JsonToString(value,text,error))
            return AnsiString();
        return AnsiString(reinterpret_cast<const char*>(text.c_str()),text.Length());
    }

    // AnsiString（旧版 JSON 生产者输出）→ json；解析失败返回可见的错误对象
    inline nlohmann::json DebugJsonFromText(const AnsiString &text)
    {
        nlohmann::json value;
        OSString error;
        if(ParseJson(value,text.c_str(),text.Length(),error))
            return value;
        return nlohmann::json{{"valid",false},{"error","internal_json_parse_error"}};
    }
}//namespace hgl::graph::debug_bridge