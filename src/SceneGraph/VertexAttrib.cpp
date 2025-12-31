#include<hgl/graph/VertexAttrib.h>
#include<hgl/graph/VKFormat.h>
#include<hgl/graph/VKVertexInputAttribute.h>

namespace hgl
{
    namespace graph
    {
        bool ParseVertexAttribType(VAType *vat,const char *str)
        {
            if(!vat||!str||!*str)return(false);

            if(*str=='f')       //float
            {
                vat->basetype=VABaseType::Float;
                vat->vec_size=1;
                return(true);
            }
            else
            if(*str=='v')       //vec
            {
                vat->basetype=VABaseType::Float;
                vat->vec_size=str[3]-'0';
            }
            else
            {
                if(*str=='i')vat->basetype=VABaseType::Int;   else
                if(*str=='u')vat->basetype=VABaseType::UInt;  else
                if(*str=='b')vat->basetype=VABaseType::Bool;  else
                if(*str=='d')vat->basetype=VABaseType::Double;else
                    return(false);

                if(str[1]!='v')
                {
                    vat->vec_size=1;
                    return(true);
                }
                
                vat->vec_size=str[4]-'0';
            }

            if(vat->vec_size<1
             ||vat->vec_size>4)return(false);

            return(true);
        }

        constexpr char vertex_attrib_vec_name[size_t(VABaseType::RANGE_SIZE)][4][8]=
        {
            {"bool",  "bvec2","bvec3","bvec4"},
            {"int",   "ivec2","ivec3","ivec4"},
            {"uint",  "uvec2","uvec3","uvec4"},
            {"float",  "vec2", "vec3", "vec4"},
            {"double","dvec2","dvec3","dvec4"}
        };

        const char *GetVertexAttribName(const VABaseType &base_type,const uint vec_size)
        {
            RANGE_CHECK_RETURN_NULLPTR(base_type)

            if(vec_size<=0||vec_size>4)return nullptr;

            return vertex_attrib_vec_name[size_t(base_type)][vec_size-1];
        }

        const char *GetVertexAttribName(const VAType *type)
        {
            if(!type||!type->Check())return(nullptr);            

            return vertex_attrib_vec_name[size_t(type->basetype)][type->vec_size-1];
        }

        namespace
        {
            constexpr VkFormat vk_format_by_basetype[][4]=
            {
                {PF_R8U,PF_RG8U,VK_FORMAT_UNDEFINED,PF_RGBA8U},    //ubyte
                {PF_R32I,PF_RG32I,PF_RGB32I,PF_RGBA32I},//int
                {PF_R32U,PF_RG32U,PF_RGB32U,PF_RGBA32U},//uint
                {PF_R32F,PF_RG32F,PF_RGB32F,PF_RGBA32F},//float
                {PF_R64F,PF_RG64F,PF_RGB64F,PF_RGBA64F} //double
            };
        }//namespace

        const VkFormat GetVulkanFormat(const VABaseType &base_type,const uint vec_size)
        {
            RANGE_CHECK_RETURN(base_type,VK_FORMAT_UNDEFINED)

            if(vec_size<=0||vec_size>4)return VK_FORMAT_UNDEFINED;

            return vk_format_by_basetype[size_t(base_type)][vec_size-1];
        }

        const VkFormat GetVulkanFormat(const VAType *type)
        {
            if(!type||!type->Check())
                return VK_FORMAT_UNDEFINED;
        
            return vk_format_by_basetype[size_t(type->basetype)][type->vec_size-1];
        }

        const VkFormat GetVulkanFormat(const VertexInputAttribute *sa)
        {
            if(!sa)return VK_FORMAT_UNDEFINED;

            RANGE_CHECK_RETURN(VABaseType(sa->basetype),VK_FORMAT_UNDEFINED)

            if(sa->vec_size<=0||sa->vec_size>4)
                return VK_FORMAT_UNDEFINED;

            return vk_format_by_basetype[size_t(sa->basetype)][sa->vec_size-1];
        }
    }//namespace graph
}//namespace hgl
