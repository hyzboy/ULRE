#ifndef HGL_SHADER_TEMPLATE_ENGINE_INCLUDE
#define HGL_SHADER_TEMPLATE_ENGINE_INCLUDE

#include<inja/inja.hpp>
#include<nlohmann/json.hpp>
#include<hgl/type/Map.h>
#include<hgl/type/List.h>
#include<hgl/type/String.h>

namespace hgl::graph
{
    using json = nlohmann::json;
    
    /**
     * Shader模块描述结构
     * 每个模块代表一个可重用的GLSL代码片段，如一个光照模型或者纹理采样函数
     */
    struct ShaderModule
    {
        AnsiString name;                        ///<模块名称
        AnsiString file_path;                   ///<模块文件路径
        AnsiString code;                        ///<GLSL代码内容
        
        AnsiStringList provides;                ///<提供的函数名列表（此模块导出的函数）
        AnsiStringList requires_funcs;          ///<依赖的函数名列表（需要其他模块提供）
        AnsiStringList requires_inputs;         ///<需要的Input变量（如Input.Normal）
        AnsiStringList requires_descriptors;    ///<需要的UBO/Sampler（如camera, sky）
        AnsiStringList dependencies;            ///<依赖的其他模块名称
    };
    
    /**
     * Shader配方结构
     * 描述如何组合多个模块来生成一个完整的shader
     */
    struct ShaderRecipe
    {
        AnsiString name;                        ///<配方名称（如"Metal", "Wood"）
        AnsiString template_file;               ///<使用的模板文件名
        
        Map<AnsiString, AnsiString> module_map; ///<模块映射："lighting" -> "blinn_phong"
        json template_data;                     ///<传递给inja的模板数据
    };
    
    /**
     * Shader模板引擎
     * 负责加载模块、解析依赖、生成shader代码
     */
    class ShaderTemplateEngine
    {
        inja::Environment env;                  ///<inja模板引擎环境
        
        AnsiString template_root;               ///<模板文件根目录
        AnsiString module_root;                 ///<模块文件根目录
        
        Map<AnsiString, ShaderModule*> module_cache;    ///<模块缓存
        Map<AnsiString, json> interface_cache;          ///<接口定义缓存
        
    public:
        /**
         * 构造函数
         * @param template_path 模板文件根路径
         * @param module_path 模块文件根路径
         */
        ShaderTemplateEngine(const AnsiString& template_path, const AnsiString& module_path);
        ~ShaderTemplateEngine();
        
        /**
         * 加载指定的模块
         * @param category 模块类别（如"lighting", "specular"）
         * @param name 模块名称（如"blinn_phong"）
         * @return 加载的模块指针，失败返回nullptr
         */
        ShaderModule* LoadModule(const AnsiString& category, const AnsiString& name);
        
        /**
         * 解析依赖树并进行拓扑排序
         * @param recipe 要处理的配方
         * @param ordered_modules 输出：排序后的模块名称列表
         * @param missing_deps 输出：缺失的依赖列表
         * @return 成功返回true，存在循环依赖或缺失依赖返回false
         */
        bool ResolveDependencies(const ShaderRecipe& recipe, 
                                AnsiStringList& ordered_modules,
                                AnsiStringList& missing_deps);
        
        /**
         * 根据配方生成最终的shader代码
         * @param recipe 材质配方
         * @return 生成的GLSL代码
         */
        AnsiString Generate(const ShaderRecipe& recipe);
        
        /**
         * 从JSON配置文件加载配方
         * @param recipe_file 配方文件路径
         * @param quality_level 质量等级（如"high", "medium", "low"）
         * @return 加载的配方
         */
        ShaderRecipe LoadRecipe(const AnsiString& recipe_file, const AnsiString& quality_level);
        
    private:
        /**
         * 构建传递给模板的数据对象
         * @param recipe 配方
         * @param ordered_modules 排序后的模块列表
         * @return inja使用的json数据
         */
        json BuildTemplateData(const ShaderRecipe& recipe, const AnsiStringList& ordered_modules);
        
        /**
         * 加载指定类别的接口定义
         * @param category 模块类别
         * @return 成功返回true
         */
        bool LoadInterface(const AnsiString& category);
    };
}//namespace hgl::graph

#endif//HGL_SHADER_TEMPLATE_ENGINE_INCLUDE
