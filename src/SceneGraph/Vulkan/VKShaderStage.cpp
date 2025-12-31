#include<hgl/graph/VKVertexInputAttribute.h>

namespace hgl
{
    namespace graph
    {
        namespace
        {
            struct ShaderStageFlagBitName
            {
               VkShaderStageFlagBits    flag;
               const char               name[32];
            };//struct ShaderStageFlagBitName

            constexpr const ShaderStageFlagBitName shader_stage_name_list[]=
            {
                {VK_SHADER_STAGE_VERTEX_BIT,                    "Vertex"        },
                {VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,      "TessControl"   },
                {VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,   "TeseEval"      },
                {VK_SHADER_STAGE_GEOMETRY_BIT,                  "Geometry"      },
                {VK_SHADER_STAGE_FRAGMENT_BIT,                  "Fragment"      },
                {VK_SHADER_STAGE_COMPUTE_BIT,                   "Compute"       },
                {VK_SHADER_STAGE_TASK_BIT_NV,                   "Task"          },
                {VK_SHADER_STAGE_MESH_BIT_NV,                   "Mesh"          },
                {VK_SHADER_STAGE_RAYGEN_BIT_KHR,                "Raygen"        },
                {VK_SHADER_STAGE_ANY_HIT_BIT_KHR,               "AnyHit"        },
                {VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,           "ClosestHit"    },
                {VK_SHADER_STAGE_MISS_BIT_KHR,                  "Miss"          },
                {VK_SHADER_STAGE_INTERSECTION_BIT_KHR,          "Intersection"  },
                {VK_SHADER_STAGE_CALLABLE_BIT_KHR,              "Callable"      },
                {VK_SHADER_STAGE_SUBPASS_SHADING_BIT_HUAWEI,    "SubpassShading"},
                {VK_SHADER_STAGE_CLUSTER_CULLING_BIT_HUAWEI,    "ClusterCulling"}
            };//

            constexpr const char UNKNOW_SHADER_STAGE_NAME[]="Unknow";
        }

        const char *GetShaderStageName(const VkShaderStageFlagBits &type)
        {
            if(type==0||type>VK_SHADER_STAGE_SUBPASS_SHADING_BIT_HUAWEI)
                return(UNKNOW_SHADER_STAGE_NAME);

            for(auto &ss:shader_stage_name_list)
                if(type==ss.flag)
                    return ss.name;

            return UNKNOW_SHADER_STAGE_NAME;
        }
        
        const uint GetShaderStageFlagBits(const char *str,int len)
        {
            if(!str||!*str)return(0);

            if(len<=0)
                len=hgl::strlen(str);

            if(len<=3)
                return(0);

            for(auto &ss:shader_stage_name_list)
                if(hgl::stricmp(str,ss.name,len)==0)
                    return ss.flag;

            return(0);
        }

        const uint GetShaderCountByBits(const uint32_t bits)
        {
            uint result=0;

            for(auto &ss:shader_stage_name_list)
            {
                if(bits&ss.flag)
                    ++result;
            }

            return result;
        }
        
        const uint GetMaxShaderStage(const uint32_t bits)
        {
            uint result=0;

            for(auto &ss:shader_stage_name_list)
            {
                if(bits&ss.flag)
                    result=ss.flag;
            }

            return result;
        }
    }//namespace graph
}//namespace hgl
