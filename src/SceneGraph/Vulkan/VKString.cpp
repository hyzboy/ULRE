#include<hgl/graph/VKString.h>
#include<hgl/type/StrChar.h>

VK_NAMESPACE_BEGIN

class IVulkanEnumStringList
{
public:

    IVulkanEnumStringList()=default;
    virtual ~IVulkanEnumStringList()=default;

    virtual const char *to(const int &value)const=0;
    virtual const int to(const char *str,const int default_value=-1)const=0;
};//class IVulkanEnumStringList

template<typename E> IVulkanEnumStringList *VkGetEnumStringList();

namespace
{
    template<typename E> struct VkEnumString
    {
        E value;
        const char *str;
    };

    template<typename E>
    class VkEnumStringList:public IVulkanEnumStringList
    {
        const VkEnumString<E> *list;
        int count;

        E def_value;

    public:

        VkEnumStringList(const VkEnumString<E> *es_array,const int es_count,const E def)
        {
            list=es_array;
            count=es_count;
            def_value=def;
        }

        const char *to(const int &value)const override
        {
            for(int i=0;i<count;i++)
            {
                if(int(list[i].value)==value)
                    return list[i].str;
            }

            return(nullptr);
        }

        const int to(const char *str,const int dv=-1)const override
        {
            for(int i=0;i<count;i++)
            {
                if(hgl::stricmp(list[i].str,str)==0)
                    return int(list[i].value);
            }

            return (dv==-1)?def_value:dv;
        }
    };//class VkEnumStringList

    const char true_string[]="true";
    const char false_string[]="false";

    template<typename E> IVulkanEnumStringList *VkGetEnumStringList();

    constexpr const VkEnumString<VkPolygonMode> vk_polygon_mode_list[]=
    {
        {VK_POLYGON_MODE_FILL,              "fill"},
        {VK_POLYGON_MODE_LINE,              "line"},
        {VK_POLYGON_MODE_POINT,             "point"},
        {VK_POLYGON_MODE_FILL_RECTANGLE_NV, "fill_rectangle"},
    };

    constexpr const VkEnumString<VkCullModeFlagBits> vk_cull_mode_list[]=
    {
        {VK_CULL_MODE_NONE,                 "none"},
        {VK_CULL_MODE_FRONT_BIT,            "front"},
        {VK_CULL_MODE_BACK_BIT,             "back"},
        {VK_CULL_MODE_FRONT_AND_BACK,       "front_and_back"},
    };

    static VkEnumStringList<VkCullModeFlagBits> VkCullModeFlagBitsESL(vk_cull_mode_list,sizeof(vk_cull_mode_list)/sizeof(VkEnumString<VkCullModeFlagBits>),VK_CULL_MODE_BACK_BIT);

    constexpr const VkEnumString<VkFrontFace> vk_front_face_list[]=
    {
        {VK_FRONT_FACE_COUNTER_CLOCKWISE,   "ccw"},
        {VK_FRONT_FACE_CLOCKWISE,           "cw"},

        {VK_FRONT_FACE_COUNTER_CLOCKWISE,   "CounterClockWise"},
        {VK_FRONT_FACE_CLOCKWISE,           "CloseWise"},
    };

    constexpr const VkEnumString<VkStencilOp> vk_stencil_op_list[]=
    {
        {VK_STENCIL_OP_KEEP,                "keep"},

        {VK_STENCIL_OP_ZERO,                "zero"},
        {VK_STENCIL_OP_ZERO,                "0"},

        {VK_STENCIL_OP_REPLACE,             "replace"},

        {VK_STENCIL_OP_INCREMENT_AND_CLAMP, "inc_clamp"},
        {VK_STENCIL_OP_DECREMENT_AND_CLAMP, "dec_clamp"},

        {VK_STENCIL_OP_INCREMENT_AND_CLAMP, "+clamp"},
        {VK_STENCIL_OP_DECREMENT_AND_CLAMP, "-clamp"},

        {VK_STENCIL_OP_INVERT,              "invert"},

        {VK_STENCIL_OP_INCREMENT_AND_WRAP,  "inc_wrap"},
        {VK_STENCIL_OP_DECREMENT_AND_WRAP,  "dec_wrap"},

        {VK_STENCIL_OP_INCREMENT_AND_WRAP,  "+wrap"},
        {VK_STENCIL_OP_DECREMENT_AND_WRAP,  "-wrap"},
    };

    constexpr const VkEnumString<VkCompareOp> vk_compare_op_list[]=
    {
        {VK_COMPARE_OP_NEVER,               "never"},

        {VK_COMPARE_OP_LESS,                "less"},
        {VK_COMPARE_OP_EQUAL,               "equal"},
        {VK_COMPARE_OP_LESS_OR_EQUAL,       "less_or_equal"},
        {VK_COMPARE_OP_GREATER,             "greater"},
        {VK_COMPARE_OP_NOT_EQUAL,           "not_equal"},
        {VK_COMPARE_OP_GREATER_OR_EQUAL,    "greater_or_equal"},

        {VK_COMPARE_OP_LESS,                "<"},
        {VK_COMPARE_OP_EQUAL,               "=="},
        {VK_COMPARE_OP_LESS_OR_EQUAL,       "<="},
        {VK_COMPARE_OP_GREATER,             ">"},
        {VK_COMPARE_OP_NOT_EQUAL,           "!="},
        {VK_COMPARE_OP_GREATER_OR_EQUAL,    ">="},

        {VK_COMPARE_OP_ALWAYS,              "always"},
    };

    constexpr const VkEnumString<VkLogicOp> vk_logic_op_list[]=
    {
        {VK_LOGIC_OP_CLEAR,                 "clear"},
        {VK_LOGIC_OP_AND,                   "and"},
        {VK_LOGIC_OP_AND_REVERSE,           "and_reverse"},
        {VK_LOGIC_OP_COPY,                  "copy"},
        {VK_LOGIC_OP_AND_INVERTED,          "and_inverted"},
        {VK_LOGIC_OP_NO_OP,                 "no_op"},
        {VK_LOGIC_OP_XOR,                   "xor"},
        {VK_LOGIC_OP_OR,                    "or"},
        {VK_LOGIC_OP_NOR,                   "nor"},
        {VK_LOGIC_OP_EQUIVALENT,            "equivalent"},
        {VK_LOGIC_OP_INVERT,                "invert"},
        {VK_LOGIC_OP_OR_REVERSE,            "or_reverse"},
        {VK_LOGIC_OP_COPY_INVERTED,         "copy_inverted"},
        {VK_LOGIC_OP_OR_INVERTED,           "or_inverted"},
        {VK_LOGIC_OP_NAND,                  "nand"},
        {VK_LOGIC_OP_SET,                   "set"},
    };

    constexpr const VkEnumString<VkBlendFactor> vk_blend_factor_list[]=
    {
        {VK_BLEND_FACTOR_ZERO,                      "zero"},
        {VK_BLEND_FACTOR_ONE,                       "one"},

        {VK_BLEND_FACTOR_ZERO,                      "0"},
        {VK_BLEND_FACTOR_ONE,                       "1"},

        {VK_BLEND_FACTOR_SRC_COLOR,                 "src_color"},
        {VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,       "one_minus_src_color"},
        {VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,       "1-src_color"},

        {VK_BLEND_FACTOR_DST_COLOR,                 "dst_color"},
        {VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,       "one_minus_dst_color"},
        {VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,       "1-dst_color"},

        {VK_BLEND_FACTOR_SRC_ALPHA,                 "src_alpha"},
        {VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,       "one_minus_src_alpha"},
        {VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,       "1-src_alpha"},

        {VK_BLEND_FACTOR_DST_ALPHA,                 "dst_alpha"},
        {VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,       "one_minus_dst_alpha"},
        {VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,       "1-dst_alpha"},

        {VK_BLEND_FACTOR_CONSTANT_COLOR,            "constant_color"},
        {VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR,  "one_minus_constant_color"},
        {VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR,  "1-constant_color"},

        {VK_BLEND_FACTOR_CONSTANT_ALPHA,            "constant_alpha"},
        {VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA,  "one_minus_constant_alpha"},
        {VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA,  "1-constant_alpha"},

        {VK_BLEND_FACTOR_SRC_ALPHA_SATURATE,        "src_alpha_saturate"},
        {VK_BLEND_FACTOR_SRC1_COLOR,                "src1_color"},
        {VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR,      "one_minus_src1_color"},
        {VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR,      "1-src1_color"},

        {VK_BLEND_FACTOR_SRC1_ALPHA,                "src1_alpha"},
        {VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA,      "one_minus_src1_alpha"},
        {VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA,      "1-src1_alpha"},
    };

    constexpr const VkEnumString<VkBlendOp> vk_blend_op_list[]=
    {
        {VK_BLEND_OP_ADD,               "add"},
        {VK_BLEND_OP_SUBTRACT,          "subtract"},
        {VK_BLEND_OP_REVERSE_SUBTRACT,  "reverse_subtract"},
        {VK_BLEND_OP_MIN,               "min"},
        {VK_BLEND_OP_MAX,               "max"},
        {VK_BLEND_OP_ZERO_EXT,          "zero"},
        {VK_BLEND_OP_SRC_EXT,           "src"},
        {VK_BLEND_OP_DST_EXT,           "dst"},
        {VK_BLEND_OP_SRC_OVER_EXT,      "src_over"},
        {VK_BLEND_OP_DST_OVER_EXT,      "dst_over"},
        {VK_BLEND_OP_SRC_IN_EXT,        "src_in"},
        {VK_BLEND_OP_DST_IN_EXT,        "dst_in"},
        {VK_BLEND_OP_SRC_OUT_EXT,       "src_out"},
        {VK_BLEND_OP_DST_OUT_EXT,       "dst_out"},
        {VK_BLEND_OP_SRC_ATOP_EXT,      "src_atop"},
        {VK_BLEND_OP_DST_ATOP_EXT,      "dst_atop"},
        {VK_BLEND_OP_XOR_EXT,           "xor"},
        {VK_BLEND_OP_MULTIPLY_EXT,      "multiply"},
        {VK_BLEND_OP_SCREEN_EXT,        "screen"},
        {VK_BLEND_OP_OVERLAY_EXT,       "overlay"},
        {VK_BLEND_OP_DARKEN_EXT,        "darken"},
        {VK_BLEND_OP_LIGHTEN_EXT,       "lighten"},
        {VK_BLEND_OP_COLORDODGE_EXT,    "colordodge"},
        {VK_BLEND_OP_COLORBURN_EXT,     "colorburn"},
        {VK_BLEND_OP_HARDLIGHT_EXT,     "hardlight"},
        {VK_BLEND_OP_SOFTLIGHT_EXT,     "softlight"},
        {VK_BLEND_OP_DIFFERENCE_EXT,    "difference"},
        {VK_BLEND_OP_EXCLUSION_EXT,     "exclusion"},
        {VK_BLEND_OP_INVERT_EXT,        "invert"},
        {VK_BLEND_OP_INVERT_RGB_EXT,    "invert_rgb"},
        {VK_BLEND_OP_LINEARDODGE_EXT,   "lineardodge"},
        {VK_BLEND_OP_LINEARBURN_EXT,    "linearburn"},
        {VK_BLEND_OP_VIVIDLIGHT_EXT,    "vividlight"},
        {VK_BLEND_OP_LINEARLIGHT_EXT,   "linearlight"},
        {VK_BLEND_OP_PINLIGHT_EXT,      "pinlight"},
        {VK_BLEND_OP_HARDMIX_EXT,       "hardmix"},
        {VK_BLEND_OP_HSL_HUE_EXT,       "hsl_hue"},
        {VK_BLEND_OP_HSL_SATURATION_EXT,"hsl_saturation"},
        {VK_BLEND_OP_HSL_COLOR_EXT,     "hsl_color"},
        {VK_BLEND_OP_HSL_LUMINOSITY_EXT,"hsl_luminosity"},
        {VK_BLEND_OP_PLUS_EXT,          "plus"},
        {VK_BLEND_OP_PLUS_CLAMPED_EXT,  "plus_clamped"},
        {VK_BLEND_OP_PLUS_CLAMPED_ALPHA_EXT,"plus_clamped_alpha"},
        {VK_BLEND_OP_PLUS_DARKER_EXT,   "plus_darker"},
        {VK_BLEND_OP_MINUS_EXT,         "minus"},
        {VK_BLEND_OP_MINUS_CLAMPED_EXT, "minus_clamped"},
        {VK_BLEND_OP_CONTRAST_EXT,      "contrast"},
        {VK_BLEND_OP_INVERT_OVG_EXT,    "invert_ovg"},
        {VK_BLEND_OP_RED_EXT,           "red"},
        {VK_BLEND_OP_GREEN_EXT,         "green"},
        {VK_BLEND_OP_BLUE_EXT,          "blue"}
    };

    constexpr const VkEnumString<VkDynamicState> vk_dynamic_state_list[]=
    {
        {VK_DYNAMIC_STATE_VIEWPORT,         "viewport"},
        {VK_DYNAMIC_STATE_SCISSOR,          "scissor"},
        {VK_DYNAMIC_STATE_LINE_WIDTH,       "line_width"},
        {VK_DYNAMIC_STATE_DEPTH_BIAS,       "depth_bias"},
        {VK_DYNAMIC_STATE_BLEND_CONSTANTS,  "blend_constants"},
        {VK_DYNAMIC_STATE_DEPTH_BOUNDS,     "depth_bounds"},
        {VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,"stencil_compare_mask"},
        {VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,"stencil_write_mask"},
        {VK_DYNAMIC_STATE_STENCIL_REFERENCE, "stencil_reference"},

        {VK_DYNAMIC_STATE_CULL_MODE,        "cull_mode"},
        {VK_DYNAMIC_STATE_FRONT_FACE,       "front_face"},
        {VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY,"primitive_topology"},
        {VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,"viewport_with_count"},
        {VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT,"scissor_with"},
        {VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE,"vertex_input_binding_stride"},

        {VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,"depth_test_enable"},
        {VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,"depth_write_enable"},
        {VK_DYNAMIC_STATE_DEPTH_COMPARE_OP, "depth_compare_op"},
        {VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE,"depth_bounds_test_enable"},

        {VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE,"stencil_test_enable"},
        {VK_DYNAMIC_STATE_STENCIL_OP, "stencil_op"},

        {VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE,"rasterizer_discard_enable"},
        {VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE,"depth_bias_enable"},
        {VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE,"primitive_restart_enable"},
        {VK_DYNAMIC_STATE_LINE_STIPPLE,"line_stipple"},

        {VK_DYNAMIC_STATE_VIEWPORT_W_SCALING_NV,"viewport_w_scaling_nv"},

        {VK_DYNAMIC_STATE_DISCARD_RECTANGLE_EXT,"discard_rectangle_ext"},
        {VK_DYNAMIC_STATE_DISCARD_RECTANGLE_ENABLE_EXT,"discard_rectangle_enable_ext"},
        {VK_DYNAMIC_STATE_DISCARD_RECTANGLE_MODE_EXT,"discard_rectangle_mode_ext"},

        {VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_EXT,"sample_locations_ext"},
        {VK_DYNAMIC_STATE_RAY_TRACING_PIPELINE_STACK_SIZE_KHR,"ray_tracing_pipeline_stack_size_khr"},

        {VK_DYNAMIC_STATE_VIEWPORT_SHADING_RATE_PALETTE_NV,"viewport_shading_rate_palette_nv"},
        {VK_DYNAMIC_STATE_VIEWPORT_COARSE_SAMPLE_ORDER_NV,"viewport_coarse_sample_order_nv"},

        {VK_DYNAMIC_STATE_EXCLUSIVE_SCISSOR_ENABLE_NV,"exclusive_scissor_enable_nv"},
        {VK_DYNAMIC_STATE_EXCLUSIVE_SCISSOR_NV,"exclusive_scissor_nv"},

        {VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR,"fragment_shading_rate_khr"},
        {VK_DYNAMIC_STATE_VERTEX_INPUT_EXT,"vertex_input_ext"},
        {VK_DYNAMIC_STATE_PATCH_CONTROL_POINTS_EXT,"patch_control_points_ext"},

        {VK_DYNAMIC_STATE_LOGIC_OP_EXT,"logic_op_ext"},
        {VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT,"color_write_enable_ext"},
        {VK_DYNAMIC_STATE_DEPTH_CLAMP_ENABLE_EXT,"depth_clamp_enable_ext"},
        {VK_DYNAMIC_STATE_POLYGON_MODE_EXT,"polygon_mode_ext"},
        {VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT,"rasterization_samples_ext"},
        {VK_DYNAMIC_STATE_SAMPLE_MASK_EXT,"sample_mask_ext"},

        {VK_DYNAMIC_STATE_ALPHA_TO_COVERAGE_ENABLE_EXT,"alpha_to_coverage_enable_ext"},
        {VK_DYNAMIC_STATE_ALPHA_TO_ONE_ENABLE_EXT,"alpha_to_one_enable_ext"},

        {VK_DYNAMIC_STATE_LOGIC_OP_ENABLE_EXT,"logic_op_enable_ext"},
        {VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT,"color_blend_enable_ext"},
        {VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT,"color_blend_equation_ext"},
        {VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT,"color_write_mask_ext"},

        {VK_DYNAMIC_STATE_TESSELLATION_DOMAIN_ORIGIN_EXT,"tessellation_domain_origin_ext"},
        {VK_DYNAMIC_STATE_RASTERIZATION_STREAM_EXT,"rasterization_stream_ext"},
        {VK_DYNAMIC_STATE_CONSERVATIVE_RASTERIZATION_MODE_EXT,"conservative_rasterization_mode_ext"},
        {VK_DYNAMIC_STATE_EXTRA_PRIMITIVE_OVERESTIMATION_SIZE_EXT,"extra_primitive_overestimation_size_ext"},

        {VK_DYNAMIC_STATE_DEPTH_CLIP_ENABLE_EXT,"depth_clip_enable_ext"},
        {VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_ENABLE_EXT,"sample_locations_enable_ext"},
        {VK_DYNAMIC_STATE_COLOR_BLEND_ADVANCED_EXT,"color_blend_advanced_ext"},
        {VK_DYNAMIC_STATE_PROVOKING_VERTEX_MODE_EXT,"provoking_vertex_mode_ext"},
        {VK_DYNAMIC_STATE_LINE_RASTERIZATION_MODE_EXT,"line_rasterization_mode_ext"},
        {VK_DYNAMIC_STATE_LINE_STIPPLE_ENABLE_EXT,"line_stipple_enable_ext"},
        {VK_DYNAMIC_STATE_DEPTH_CLIP_NEGATIVE_ONE_TO_ONE_EXT,"depth_clip_negative_one_to_one_ext"},
        {VK_DYNAMIC_STATE_VIEWPORT_W_SCALING_ENABLE_NV,"viewport_w_scaling_enable_nv"},
        {VK_DYNAMIC_STATE_VIEWPORT_SWIZZLE_NV,"viewport_swizzle_nv"},

        {VK_DYNAMIC_STATE_COVERAGE_TO_COLOR_ENABLE_NV,"coverage_to_color_enable_nv"},
        {VK_DYNAMIC_STATE_COVERAGE_TO_COLOR_LOCATION_NV,"coverage_to_color_location_nv"},
        {VK_DYNAMIC_STATE_COVERAGE_MODULATION_MODE_NV,"coverage_modulation_mode_nv"},
        {VK_DYNAMIC_STATE_COVERAGE_MODULATION_TABLE_ENABLE_NV,"coverage_modulation_table_enable_nv"},
        {VK_DYNAMIC_STATE_COVERAGE_MODULATION_TABLE_NV,"coverage_modulation_table_nv"},

        {VK_DYNAMIC_STATE_SHADING_RATE_IMAGE_ENABLE_NV,"shading_rate_image_enable_nv"},
        {VK_DYNAMIC_STATE_REPRESENTATIVE_FRAGMENT_TEST_ENABLE_NV,"representative_fragment_test_enable_nv"},
        {VK_DYNAMIC_STATE_COVERAGE_REDUCTION_MODE_NV,"coverage_reduction_mode_nv"},
        {VK_DYNAMIC_STATE_ATTACHMENT_FEEDBACK_LOOP_ENABLE_EXT,"attachment_feedback_loop_enable_ext"},
        {VK_DYNAMIC_STATE_DEPTH_CLAMP_RANGE_EXT,"depth_clamp_range_ext"}
    };
}//namespace

#define VK_ENUM_STRING_LIST(VK_TYPE,VK_LIST,VK_DEFAULT)    \
    static VkEnumStringList<Vk##VK_TYPE> Vk##VK_TYPE##ESL(vk_##VK_LIST##_list,sizeof(vk_##VK_LIST##_list)/sizeof(VkEnumString<Vk##VK_TYPE>),VK_DEFAULT);   \
    template<> IVulkanEnumStringList *VkGetEnumStringList<Vk##VK_TYPE>(){return &Vk##VK_TYPE##ESL;} \
    template<> const char *VkEnum2String(const Vk##VK_TYPE &value){return Vk##VK_TYPE##ESL.to(value);} \
    template<> const Vk##VK_TYPE String2VkEnum(const char *str){return (Vk##VK_TYPE)Vk##VK_TYPE##ESL.to(str);}

    VK_ENUM_STRING_LIST(PolygonMode,        polygon_mode,   VK_POLYGON_MODE_FILL)
    VK_ENUM_STRING_LIST(FrontFace,          front_face,     VK_FRONT_FACE_CLOCKWISE)
    VK_ENUM_STRING_LIST(StencilOp,          stencil_op,     VK_STENCIL_OP_KEEP)
    VK_ENUM_STRING_LIST(CompareOp,          compare_op,     VK_COMPARE_OP_NEVER)
    VK_ENUM_STRING_LIST(LogicOp,            logic_op,       VK_LOGIC_OP_CLEAR)
    VK_ENUM_STRING_LIST(BlendFactor,        blend_factor,   VK_BLEND_FACTOR_ZERO)
    VK_ENUM_STRING_LIST(BlendOp,            blend_op,       VK_BLEND_OP_ADD)
    VK_ENUM_STRING_LIST(DynamicState,       dynamic_state,  VK_DYNAMIC_STATE_VIEWPORT)

#undef VK_ENUM_STRING_LIST

template<> const char *VkEnum2String(const VkBool32 &value)
{
    return value?true_string:false_string;
}

template<> const VkBool32 String2VkEnum(const char *str)
{
    if(hgl::stricmp(str,true_string)==0)return VK_TRUE;
    if(hgl::stricmp(str,false_string)==0)return VK_FALSE;
    return VK_FALSE;
}

const char *VkCullMode2String(const VkCullModeFlags &cull)
{
    return VkCullModeFlagBitsESL.to(cull);
}

const VkCullModeFlags String2VkCullMode(const char *str)
{
    return VkCullModeFlagBitsESL.to(str,VK_CULL_MODE_BACK_BIT);
}

VK_NAMESPACE_END
