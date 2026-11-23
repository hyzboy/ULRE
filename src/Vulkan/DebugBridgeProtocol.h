#pragma once

#include<cstddef>
#include<cstdint>
#include<cstring>

#include<hgl/type/String.h>
#include<hgl/util/json/JsonTool.h>

namespace hgl::graph::debug_bridge
{
    /**
     * Debug Bridge JSON-RPC 2.0 请求（严格解析）。
     *
     * 解析后端为 CMUtil JsonTool 的 nlohmann::json，取代旧手写解析器。
     * 相对旧实现修复两处缺陷：
     *  - 顶层 id 与 params 内 name/entity/id 不再共享状态：旧实现两者同时出现
     *    必然解析失败（带实体名的 trace_entity_pipeline 无法使用）；
     *  - \uXXXX 转义正确解码为 UTF-8（旧实现将非 ASCII 截断为 0xFF）。
     *
     * 保留的严格性（TestDebugBridgeLifecycle 覆盖）：
     *  - jsonrpc 必须为 "2.0"；method 与 id 必填；id 仅允许 string/number/null；
     *  - params 必须为对象；重复键拒绝（比旧实现更严：旧实现仅拒绝已知键重复）；
     *  - target_id/request_id/resource_id/offset/size 必须为无符号整数
     *    （拒绝负数/小数/指数/溢出）；
     *  - mode/attachment/name/entity/id 必须为字符串；实体三别名互斥；
     *  - 容器嵌套深度 ≤32；拒绝尾随多余内容、前导零、孤立代理项、裸控制字符。
     */
    class Request
    {
        nlohmann::json document;
        nlohmann::json request_id;
        const nlohmann::json *params=nullptr;
        AnsiString method_name;

        // 已知字符串字段不得含嵌入 NUL（与旧实现一致）
        static bool IsPlainString(const nlohmann::json &value)
        {
            if(!value.is_string())
                return false;
            const auto &text=value.get_ref<const nlohmann::json::string_t&>();
            return ::strlen(text.c_str())==text.size();
        }

        bool Validate()
        {
            if(!document.is_object())
                return false;

            const auto version=document.find("jsonrpc");
            if(version==document.end() || *version!=nlohmann::json("2.0"))
                return false;

            const auto method=document.find("method");
            if(method==document.end() || !IsPlainString(*method))
                return false;

            const auto id=document.find("id");
            if(id==document.end())
                return false;
            if(id->is_string())
            {
                if(!IsPlainString(*id))
                    return false;
            }
            else if(!id->is_number() && !id->is_null())
                return false;

            const auto params_it=document.find("params");
            if(params_it!=document.end())
            {
                if(!params_it->is_object())
                    return false;
                params=&*params_it;

                static const char *unsigned_keys[]=
                    {"target_id","request_id","resource_id","offset","size"};
                for(const char *key:unsigned_keys)
                {
                    const auto it=params->find(key);
                    if(it!=params->end() && !it->is_number_unsigned())
                        return false;
                }

                static const char *string_keys[]=
                    {"mode","attachment","name","entity","id"};
                int entity_aliases=0;
                for(const char *key:string_keys)
                {
                    const auto it=params->find(key);
                    if(it==params->end())
                        continue;
                    if(!IsPlainString(*it))
                        return false;
                    if(::strcmp(key,"name")==0 || ::strcmp(key,"entity")==0 || ::strcmp(key,"id")==0)
                        ++entity_aliases;
                }
                if(entity_aliases>1)
                    return false;
            }

            request_id=*id;
            const auto &method_text=method->get_ref<const nlohmann::json::string_t&>();
            method_name=AnsiString(method_text.c_str(),static_cast<int>(method_text.size()));
            return true;
        }

    public:
        bool Parse(const char *text,size_t length)
        {
            document=nlohmann::json();
            request_id=nlohmann::json();
            params=nullptr;
            method_name=AnsiString();

            if(!text || length==0)
                return false;

            int container_depth=0;
            int object_depth=0;
            bool failed=false;
            nlohmann::json key_sets[33];    // 按对象深度(1..32)索引，用于重复键检测

            auto callback=[&](int,nlohmann::json::parse_event_t event,nlohmann::json &parsed)->bool
            {
                if(failed)
                    return true;
                switch(event)
                {
                    case nlohmann::json::parse_event_t::object_start:
                        ++container_depth;
                        if(container_depth>32) { failed=true; break; }
                        ++object_depth;
                        key_sets[object_depth]=nlohmann::json::object();
                        break;
                    case nlohmann::json::parse_event_t::array_start:
                        ++container_depth;
                        if(container_depth>32) failed=true;
                        break;
                    case nlohmann::json::parse_event_t::object_end:
                        --container_depth;
                        --object_depth;
                        break;
                    case nlohmann::json::parse_event_t::array_end:
                        --container_depth;
                        break;
                    case nlohmann::json::parse_event_t::key:
                    {
                        if(object_depth<1 || object_depth>32) { failed=true; break; }
                        const auto &key=parsed.get_ref<const nlohmann::json::string_t&>();
                        nlohmann::json &set=key_sets[object_depth];
                        if(set.contains(key.c_str()))
                            failed=true;
                        else
                            set[key.c_str()]=true;
                        break;
                    }
                    default:
                        break;
                }
                return true;
            };

            document=nlohmann::json::parse(text,text+length,callback,false);
            if(failed || document.is_discarded())
                return false;
            return Validate();
        }

        const AnsiString &GetMethod()const { return method_name; }
        const nlohmann::json &GetId()const { return request_id; }

        AnsiString GetParamString(const char *key,const char *fallback="")const
        {
            if(params)
            {
                const auto it=params->find(key);
                if(it!=params->end() && it->is_string())
                {
                    const auto &text=it->get_ref<const nlohmann::json::string_t&>();
                    return AnsiString(text.c_str(),static_cast<int>(text.size()));
                }
            }
            return AnsiString(fallback);
        }

        bool HasParam(const char *key)const
        {
            return params && params->is_object() && params->contains(key);
        }

        bool GetParamUInt64(const char *key,uint64_t &value)const
        {
            if(params)
            {
                const auto it=params->find(key);
                if(it!=params->end() && it->is_number_unsigned())
                {
                    value=it->get<uint64_t>();
                    return true;
                }
            }
            return false;
        }

        // 实体参数：按 name → entity → id 取第一个存在者（三者互斥）
        AnsiString GetEntity()const
        {
            if(params)
            {
                if(params->contains("name"))   return GetParamString("name");
                if(params->contains("entity")) return GetParamString("entity");
                if(params->contains("id"))     return GetParamString("id");
            }
            return AnsiString();
        }
    };
}//namespace hgl::graph::debug_bridge
