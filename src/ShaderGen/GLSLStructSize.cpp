#include <string>
#include <string_view>
#include <cctype>
#include <cstdint>
#include <vector>
#include <optional>
#include <algorithm>
#include <cassert>

/*
 CalculateGLSLStructSize
 - Input: GLSL struct text that starts with '{' and ends with '}' (may contain comments).
 - Output: total byte size when the struct is laid out using std140 (UBO) rules.
 - Supports: scalars (float/int/uint/bool), vectors (vec2/vec3/vec4, ivec*, uvec*, bvec*),
   matrices (matNxM and matN, matCxR semantics), arrays (e.g. vec3 arr[10];),
   nested inline structs (struct { ... } name;).
 - Returns 0 on parse error.
*/

static inline std::size_t round_up(std::size_t v,std::size_t a)
{
    return (v+a-1)/a*a;
}

struct TypeInfo
{
    std::size_t size;      // occupied bytes
    std::size_t align;     // base alignment
};

class Parser
{
    std::string_view s;
    std::size_t i=0;
    std::size_t n=0;

public:
    Parser(std::string_view src): s(src),i(0),n(src.size()) {}

    void skip_space()
    {
        while (i<n)
        {
            if (std::isspace((unsigned char)s[i])) { ++i; continue; }
            // single-line comment
            if (s[i]=='/'&&i+1<n&&s[i+1]=='/')
            {
                i+=2;
                while (i<n&&s[i]!='\n') ++i;
                continue;
            }
            // multi-line comment
            if (s[i]=='/'&&i+1<n&&s[i+1]=='*')
            {
                i+=2;
                while (i+1<n&&!(s[i]=='*'&&s[i+1]=='/')) ++i;
                if (i+1<n) i+=2;
                continue;
            }
            break;
        }
    }

    bool starts_with(char c)
    {
        skip_space();
        return (i<n&&s[i]==c);
    }

    bool consume_if(char c)
    {
        skip_space();
        if (i<n&&s[i]==c) { ++i; return true; }
        return false;
    }

    std::optional<std::string> parse_ident()
    {
        skip_space();
        if (i>=n||!(std::isalpha((unsigned char)s[i])||s[i]=='_')) return std::nullopt;
        std::size_t st=i;
        ++i;
        while (i<n&&(std::isalnum((unsigned char)s[i])||s[i]=='_')) ++i;
        return std::string(s.substr(st,i-st));
    }

    // read a numeric literal (for array size). returns 0 on failure.
    uint64_t parse_uint_literal()
    {
        skip_space();
        if (i>=n||!std::isdigit((unsigned char)s[i])) return 0;
        uint64_t v=0;
        while (i<n&&std::isdigit((unsigned char)s[i]))
        {
            v=v*10+(s[i]-'0');
            ++i;
        }
        return v;
    }

    // parse until semicolon, returning the substring contents (starting at current i).
    std::string_view read_until_semicolon()
    {
        skip_space();
        std::size_t st=i;
        while (i<n&&s[i]!=';') ++i;
        if (i<n&&s[i]==';')
        {
            std::size_t len=i-st;
            ++i; // consume ';'
            return s.substr(st,len);
        }
        return std::string_view();
    }

    // Main entry: expects to be positioned at '{' or before it.
    std::optional<TypeInfo> parse_struct_block()
    {
        skip_space();
        if (i>=n||s[i]!='{') return std::nullopt;
        ++i; // skip '{'
        // parse members until matching '}'
        std::vector<TypeInfo> memberInfos;
        std::size_t offset=0;
        while (true)
        {
            skip_space();
            if (i>=n) return std::nullopt;
            if (s[i]=='}') { ++i; break; }
            // If there's a nested 'struct' inline
            auto saved=i;
            auto ident=parse_ident();
            if (!ident) return std::nullopt;
            if (*ident=="struct")
            {
                // Could be: struct Name { ... } varname;  OR struct { ... } varname;
                skip_space();
                std::optional<std::string> nextIdent=std::nullopt;
                if (i<n&&(std::isalpha((unsigned char)s[i])||s[i]=='_'))
                {
                    nextIdent=parse_ident();
                }
                skip_space();
                std::optional<TypeInfo> nestedType;
                if (i<n&&s[i]=='{')
                {
                    // inline definition
                    nestedType=parse_struct_block();
                    if (!nestedType) return std::nullopt;
                    // after struct block there may be a name and array spec and semicolon
                    skip_space();
                    // Parse one or more declarators before ';' (we only calculate for first variable)
                    // Read the declarator substring
                    auto decl=read_until_semicolon();
                    if (decl.empty()) return std::nullopt;
                    // parse variable name and optional [N]
                    // find first identifier inside decl from end
                    std::size_t pos=decl.size();
                    // walk backwards to find identifier start
                    while (pos>0&&std::isspace((unsigned char)decl[pos-1])) --pos;
                    // find identifier end (before '[' or whitespace)
                    std::size_t idend=pos;
                    while (idend>0&&(std::isalnum((unsigned char)decl[idend-1])||decl[idend-1]=='_')) --idend;
                    if (idend==pos)
                    {
                        // no name? treat as anonymous (like an unnamed struct array?) -> skip (error)
                        return std::nullopt;
                    }
                    // identifier string optional; check for array
                    std::size_t arrStartPos=idend+(pos-idend);
                    // simpler: just search for '[' in decl
                    std::size_t bracket=decl.find('[');
                    uint64_t arrayCount=0;
                    if (bracket!=std::string_view::npos)
                    {
                        // read number between [ and ]
                        std::size_t b2=decl.find(']',bracket+1);
                        if (b2==std::string_view::npos) return std::nullopt;
                        std::string_view numsv=decl.substr(bracket+1,b2-(bracket+1));
                        // trim
                        auto beg=numsv.find_first_not_of(" \t\r\n");
                        auto end=numsv.find_last_not_of(" \t\r\n");
                        if (beg==std::string_view::npos) return std::nullopt;
                        numsv=numsv.substr(beg,end-beg+1);
                        uint64_t v=0;
                        for (char c:numsv)
                        {
                            if (!std::isdigit((unsigned char)c)) { v=0; break; }
                            v=v*10+(c-'0');
                        }
                        arrayCount=v;
                        if (arrayCount==0) return std::nullopt;
                    }
                    // compute member TypeInfo (nested struct)
                    TypeInfo elem=*nestedType;
                    if (arrayCount>0)
                    {
                        std::size_t arrAlign=round_up(elem.align,16);
                        std::size_t arrStride=round_up(elem.size,16);
                        TypeInfo t{ arrStride*arrayCount, arrAlign };
                        // place t
                        offset=round_up(offset,t.align);
                        offset+=t.size;
                        memberInfos.push_back(t);
                    }
                    else
                    {
                        // place struct member: align struct itself to its align
                        offset=round_up(offset,elem.align);
                        offset+=elem.size;
                        memberInfos.push_back(elem);
                    }
                    continue;
                }
                else
                {
                    // struct NAME var; - we don't have struct definition, unknown size -> can't compute
                    return std::nullopt;
                }
            }
            else
            {
                // not "struct", roll back and parse a regular declaration like "vec3 pos[10];"
                i=saved; // reset to beginning of the identifier
                // parse type token (could be e.g. "highp vec3" with precision qualifier; we will skip common qualifiers)
                // consume possible qualifiers (e.g., "highp", "mediump", "precision", "layout" - just skip tokens until we reach a known type token)
                // For simplicity, parse next token as type name
                auto typeTok=parse_ident();
                if (!typeTok) return std::nullopt;
                std::string typeName=*typeTok;

                // Some types may be like "mat2x3" or "dvec3" - keep as given.
                // Now read the rest of the declaration up to ';'
                auto decl=read_until_semicolon();
                if (decl.empty()) return std::nullopt;
                // decl contains from after typeToken to before ';'
                // We need to find first declarator (variable name possibly with [N])
                // Trim leading spaces
                std::size_t pos=0;
                while (pos<decl.size()&&std::isspace((unsigned char)decl[pos])) ++pos;
                // variable name starts here
                if (pos>=decl.size()) return std::nullopt;
                // variable name may be after qualifiers like "in", "out" etc, but inside struct unlikely. We'll accept first ident here.
                // read ident
                std::size_t nameStart=pos;
                while (pos<decl.size()&&(std::isalnum((unsigned char)decl[pos])||decl[pos]=='_')) ++pos;
                if (nameStart==pos) return std::nullopt;
                std::string_view varname=decl.substr(nameStart,pos-nameStart);
                // check for array
                uint64_t arrayCount=0;
                // find '[' after varname in decl
                std::size_t bracket=decl.find('[',pos);
                if (bracket!=std::string_view::npos)
                {
                    std::size_t b2=decl.find(']',bracket+1);
                    if (b2==std::string_view::npos) return std::nullopt;
                    std::string_view numsv=decl.substr(bracket+1,b2-(bracket+1));
                    // trim
                    auto beg=numsv.find_first_not_of(" \t\r\n");
                    auto end=numsv.find_last_not_of(" \t\r\n");
                    if (beg==std::string_view::npos) return std::nullopt;
                    numsv=numsv.substr(beg,end-beg+1);
                    uint64_t v=0;
                    for (char c:numsv)
                    {
                        if (!std::isdigit((unsigned char)c)) { v=0; break; }
                        v=v*10+(c-'0');
                    }
                    arrayCount=v;
                    if (arrayCount==0) return std::nullopt;
                }

                // compute type info for typeName
                auto tiOpt=type_info_from_name(typeName);
                if (!tiOpt) return std::nullopt;
                TypeInfo elem=*tiOpt;
                TypeInfo memberType;
                if (arrayCount>0)
                {
                    std::size_t arrAlign=round_up(elem.align,16);
                    std::size_t arrStride=round_up(elem.size,16);
                    memberType=TypeInfo{ arrStride*arrayCount, arrAlign };
                }
                else
                {
                    memberType=elem;
                }
                // place member: align offset then add size
                offset=round_up(offset,memberType.align);
                offset+=memberType.size;
                memberInfos.push_back(memberType);
                continue;
            }
        } // end members loop

        // compute struct alignment and size
        std::size_t maxAlign=0;
        std::size_t total=0;
        // re-layout to compute accurate align and size (we already computed placing offsets incrementally above; but to compute struct alignment we need max of member align)
        for (auto &m:memberInfos)
        {
            maxAlign=std::max(maxAlign,m.align);
        }
        if (maxAlign==0) maxAlign=16;
        std::size_t structAlign=round_up(maxAlign,16);
        std::size_t structSize=round_up(offset,16);
        return TypeInfo{ structSize, structAlign };
    }

private:
    // Helper: produce TypeInfo for a basic type or matrix
    std::optional<TypeInfo> type_info_from_name(const std::string &typeNameRaw)
    {
        std::string typeName=typeNameRaw;
        // lower-case for matching
        std::transform(typeName.begin(),typeName.end(),typeName.begin(),::tolower);

        // Scalars
        if (typeName=="float"||typeName=="int"||typeName=="uint"||typeName=="bool")
        {
            // std140 scalar = 4 bytes alignment 4
            return TypeInfo{ 4,4 };
        }
        // doubles (rare in UBOs) - treat scalar 8/8
        if (typeName=="double")
        {
            return TypeInfo{ 8,8 };
        }
        // vectors: vecN, dvecN, ivecN, uvecN, bvecN
        auto vec_match=[&](const std::string &pfx) -> std::optional<TypeInfo>
            {
                if (typeName.rfind(pfx,0)==0)
                {
                    std::string rest=typeName.substr(pfx.size());
                    if (rest=="2") return TypeInfo{ 8,8 };
                    if (rest=="3") return TypeInfo{ 16,16 }; // vec3 occupies 16 in std140
                    if (rest=="4") return TypeInfo{ 16,16 };
                }
                return std::nullopt;
            };
        if (auto t=vec_match("vec")) return t;
        if (auto t=vec_match("dvec"))
        {
            // double-vector: 2*8=16 -> alignment 16 for size >=16, vec3d -> 3*8=24 -> round up to 32? For simplicity set vec3d align=32. But double usage in std140 is rare; we approximate:
            if (typeName=="dvec2") return TypeInfo{ 16,16 };
            if (typeName=="dvec3") return TypeInfo{ 32,32 };
            if (typeName=="dvec4") return TypeInfo{ 32,32 };
            return TypeInfo{ 16,16 };
        }
        if (auto t=vec_match("ivec")) return t;
        if (auto t=vec_match("uvec")) return t;
        if (auto t=vec_match("bvec")) return t;

        // matrices: patterns matN or matCxR (mat2, mat3, mat4, mat2x3 etc)
        if (typeName.rfind("mat",0)==0)
        {
            std::string tail=typeName.substr(3); // after "mat"
            int c=0,r=0;
            auto posx=tail.find('x');
            if (posx==std::string::npos)
            {
                // matN => N x N
                if (tail.empty()) return std::nullopt;
                c=r=tail[0]-'0';
            }
            else
            {
                // matCxR
                if (posx==0||posx+1>=tail.size()) return std::nullopt;
                c=tail[0]-'0';
                r=tail[posx+1]-'0';
            }
            if (!(c>=1&&c<=4&&r>=1&&r<=4)) return std::nullopt;
            // each column is a vector of 'r' components.
            std::size_t vecSize;
            std::size_t vecAlign;
            if (r==1) { vecSize=4; vecAlign=4; }
            else if (r==2) { vecSize=8; vecAlign=8; }
            else if (r==3) { vecSize=12; vecAlign=16; } // vec3 occupies 16
            else { vecSize=16; vecAlign=16; } // r==4
            // column stride is roundUp(vecSize, 16)
            std::size_t colStride=round_up(vecSize,16);
            std::size_t totalSize=colStride*(std::size_t)c;
            // base alignment for matrix treated as array of columns -> column stride (rounded)
            std::size_t baseAlign=colStride;
            return TypeInfo{ totalSize, baseAlign };
        }

        // fallback: unknown type (e.g., user struct name, sampler, etc.) -> can't compute
        return std::nullopt;
    }
}; // end Parser

// Public API
std::size_t CalculateGLSLStructSize(const std::string &glslStructText)
{
    // find first '{' and matching '}'
    auto first=glslStructText.find('{');
    if (first==std::string::npos) return 0;
    // pass substring from first to matching end
    // Simple brace matching
    int depth=0;
    std::size_t endpos=std::string::npos;
    for (std::size_t k=first; k<glslStructText.size(); ++k)
    {
        if (glslStructText[k]=='{') ++depth;
        else if (glslStructText[k]=='}')
        {
            --depth;
            if (depth==0) { endpos=k; break; }
        }
    }
    if (endpos==std::string::npos) return 0;
    std::string_view body(glslStructText.c_str()+first,endpos-first+1);
    Parser p(body);
    auto tiOpt=p.parse_struct_block();
    if (!tiOpt) return 0;
    return tiOpt->size;
}

// Example quick test (commented out; keep for local testing)
/*
#include <iostream>
int main() {
    std::string s = "{\n    vec3 pos;\n    float time;\n    mat4 transform;\n    vec3 normals[2];\n}";
    std::cout << "size = " << CalculateGLSLStructSize(s) << "\n";
}
*/
