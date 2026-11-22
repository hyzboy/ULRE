#pragma once

#include<cstddef>
#include<cstdint>
#include<cstring>

namespace hgl::graph::debug_bridge
{
    struct Request
    {
        char method[96]{};
        char mode[64]{};
        char attachment[32]{};
        char id[256]{};
        bool has_id=false;
        uint64_t target_id=0;
        uint64_t request_id=0;
        uint64_t resource_id=0;
        uint64_t offset=0;
        uint64_t size=0;
        bool has_target_id=false;
        bool has_request_id=false;
        bool has_resource_id=false;
        bool has_offset=false;
        bool has_size=false;
    };

    class Parser
    {
        const char *cursor;
        const char *end;

        void Space()
        {
            while(cursor<end && (*cursor==' ' || *cursor=='\t' || *cursor=='\r' || *cursor=='\n'))
                ++cursor;
        }

        bool Consume(char ch)
        {
            Space();
            if(cursor==end || *cursor!=ch)
                return false;
            ++cursor;
            return true;
        }

        static int Hex(char ch)
        {
            if(ch>='0' && ch<='9') return ch-'0';
            if(ch>='a' && ch<='f') return ch-'a'+10;
            if(ch>='A' && ch<='F') return ch-'A'+10;
            return -1;
        }

        bool Unicode(unsigned &value)
        {
            if(end-cursor<4)
                return false;
            value=0;
            for(int i=0;i<4;++i)
            {
                const int digit=Hex(*cursor++);
                if(digit<0)
                    return false;
                value=(value<<4)|static_cast<unsigned>(digit);
            }
            return true;
        }

        bool String(char *out=nullptr,size_t capacity=0)
        {
            if(!Consume('"'))
                return false;
            size_t length=0;
            while(cursor<end)
            {
                unsigned ch=static_cast<unsigned char>(*cursor++);
                if(ch=='"')
                {
                    if(out)
                    {
                        if(length>=capacity) return false;
                        out[length]=0;
                    }
                    return true;
                }
                if(ch<0x20)
                    return false;
                if(ch=='\\')
                {
                    if(cursor==end) return false;
                    ch=static_cast<unsigned char>(*cursor++);
                    switch(ch)
                    {
                        case '"': case '\\': case '/': break;
                        case 'b': ch='\b'; break;
                        case 'f': ch='\f'; break;
                        case 'n': ch='\n'; break;
                        case 'r': ch='\r'; break;
                        case 't': ch='\t'; break;
                        case 'u':
                        {
                            if(!Unicode(ch)) return false;
                            if(ch>=0xd800 && ch<=0xdbff)
                            {
                                if(end-cursor<6 || cursor[0]!='\\' || cursor[1]!='u') return false;
                                cursor+=2;
                                unsigned low=0;
                                if(!Unicode(low) || low<0xdc00 || low>0xdfff) return false;
                                ch=0x10000+((ch-0xd800)<<10)+(low-0xdc00);
                            }
                            else if(ch>=0xdc00 && ch<=0xdfff)
                                return false;
                            if(ch>=128) ch=0xff;
                            break;
                        }
                        default: return false;
                    }
                }
                if(out)
                {
                    if(ch==0 || length+1>=capacity) return false;
                    out[length++]=static_cast<char>(ch);
                }
            }
            return false;
        }

        bool Number()
        {
            if(cursor<end && *cursor=='-') ++cursor;
            if(cursor==end) return false;
            if(*cursor=='0') ++cursor;
            else
            {
                if(*cursor<'1' || *cursor>'9') return false;
                do { ++cursor; } while(cursor<end && *cursor>='0' && *cursor<='9');
            }
            if(cursor<end && *cursor=='.')
            {
                ++cursor;
                if(cursor==end || *cursor<'0' || *cursor>'9') return false;
                do { ++cursor; } while(cursor<end && *cursor>='0' && *cursor<='9');
            }
            if(cursor<end && (*cursor=='e' || *cursor=='E'))
            {
                ++cursor;
                if(cursor<end && (*cursor=='+' || *cursor=='-')) ++cursor;
                if(cursor==end || *cursor<'0' || *cursor>'9') return false;
                do { ++cursor; } while(cursor<end && *cursor>='0' && *cursor<='9');
            }
            return true;
        }

        bool Unsigned(uint64_t &value)
        {
            Space();
            if(cursor==end || *cursor<'0' || *cursor>'9') return false;
            if(*cursor=='0' && cursor+1<end && cursor[1]>='0' && cursor[1]<='9')
                return false;
            value=0;
            do
            {
                const unsigned digit=static_cast<unsigned>(*cursor-'0');
                if(value>(UINT64_MAX-digit)/10) return false;
                value=value*10+digit;
                ++cursor;
            } while(cursor<end && *cursor>='0' && *cursor<='9');
            return true;
        }

        bool Literal(const char *text,size_t size)
        {
            if(static_cast<size_t>(end-cursor)<size || ::memcmp(cursor,text,size)!=0)
                return false;
            cursor+=size;
            return true;
        }

        bool Value(unsigned depth)
        {
            if(depth>32) return false;
            Space();
            if(cursor==end) return false;
            if(*cursor=='"') return String();
            if(*cursor=='{')
            {
                ++cursor;
                Space();
                if(cursor<end && *cursor=='}') { ++cursor; return true; }
                do
                {
                    if(!String() || !Consume(':') || !Value(depth+1)) return false;
                    Space();
                    if(cursor<end && *cursor=='}') { ++cursor; return true; }
                } while(Consume(','));
                return false;
            }
            if(*cursor=='[')
            {
                ++cursor;
                Space();
                if(cursor<end && *cursor==']') { ++cursor; return true; }
                do
                {
                    if(!Value(depth+1)) return false;
                    Space();
                    if(cursor<end && *cursor==']') { ++cursor; return true; }
                } while(Consume(','));
                return false;
            }
            if(*cursor=='t') return Literal("true",4);
            if(*cursor=='f') return Literal("false",5);
            if(*cursor=='n') return Literal("null",4);
            return Number();
        }

        bool Params(Request &request)
        {
            if(!Consume('{')) return false;
            Space();
            if(cursor<end && *cursor=='}') { ++cursor; return true; }
            bool has_mode=false,has_attachment=false;
            do
            {
                char key[96];
                if(!String(key,sizeof(key)) || !Consume(':')) return false;
                if(::strcmp(key,"mode")==0)
                {
                    if(has_mode || !String(request.mode,sizeof(request.mode))) return false;
                    has_mode=true;
                }
                else if(::strcmp(key,"attachment")==0)
                {
                    if(has_attachment || !String(request.attachment,sizeof(request.attachment)))
                        return false;
                    has_attachment=true;
                }
                else if(::strcmp(key,"target_id")==0)
                {
                    if(request.has_target_id || !Unsigned(request.target_id)) return false;
                    request.has_target_id=true;
                }
                else if(::strcmp(key,"request_id")==0)
                {
                    if(request.has_request_id || !Unsigned(request.request_id)) return false;
                    request.has_request_id=true;
                }
                else if(::strcmp(key,"resource_id")==0)
                {
                    if(request.has_resource_id || !Unsigned(request.resource_id)) return false;
                    request.has_resource_id=true;
                }
                else if(::strcmp(key,"offset")==0)
                {
                    if(request.has_offset || !Unsigned(request.offset)) return false;
                    request.has_offset=true;
                }
                else if(::strcmp(key,"size")==0)
                {
                    if(request.has_size || !Unsigned(request.size)) return false;
                    request.has_size=true;
                }
                else if(::strcmp(key,"name")==0 || ::strcmp(key,"entity")==0 || ::strcmp(key,"id")==0)
                {
                    if(request.has_id || !String(request.id,sizeof(request.id))) return false;
                    request.has_id=true;
                }
                else if(!Value(1)) return false;
                Space();
                if(cursor<end && *cursor=='}') { ++cursor; return true; }
            } while(Consume(','));
            return false;
        }

    public:
        Parser(const char *input,size_t length):cursor(input),end(input+length){}

        bool Parse(Request &request)
        {
            if(!Consume('{')) return false;
            bool has_method=false,has_version=false,has_params=false;
            Space();
            if(cursor<end && *cursor=='}') return false;
            do
            {
                char key[96];
                if(!String(key,sizeof(key)) || !Consume(':')) return false;
                if(::strcmp(key,"method")==0)
                {
                    if(has_method || !String(request.method,sizeof(request.method))) return false;
                    has_method=true;
                }
                else if(::strcmp(key,"jsonrpc")==0)
                {
                    char version[8];
                    if(has_version || !String(version,sizeof(version)) || ::strcmp(version,"2.0")!=0)
                        return false;
                    has_version=true;
                }
                else if(::strcmp(key,"params")==0)
                {
                    if(has_params || !Params(request)) return false;
                    has_params=true;
                }
                else if(::strcmp(key,"id")==0)
                {
                    if(request.has_id) return false;
                    Space();
                    const char *start=cursor;
                    if(cursor==end || (*cursor!='"' && *cursor!='-' &&
                       !(*cursor>='0' && *cursor<='9') && *cursor!='n')) return false;
                    if(!Value(1)) return false;
                    const size_t length=static_cast<size_t>(cursor-start);
                    if(length>=sizeof(request.id)) return false;
                    ::memcpy(request.id,start,length);
                    request.id[length]=0;
                    request.has_id=true;
                }
                else if(!Value(1)) return false;
                Space();
                if(cursor<end && *cursor=='}')
                {
                    ++cursor;
                    Space();
                    return cursor==end && has_method && has_version && request.has_id;
                }
            } while(Consume(','));
            return false;
        }
    };
}
