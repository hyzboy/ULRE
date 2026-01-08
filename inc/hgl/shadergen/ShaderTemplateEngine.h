#ifndef HGL_SHADER_TEMPLATE_ENGINE_INCLUDE
#define HGL_SHADER_TEMPLATE_ENGINE_INCLUDE

#include<inja/inja.hpp>
#include<nlohmann/json.hpp>
#include<string>
#include<vector>
#include<unordered_map>
#include<memory>

namespace hgl::graph
{
    using json = nlohmann::json;
    
    /**
     * Shader模块描述结构
     * 每个模块代表一个可重用的GLSL代码片段，如一个光照模型或者纹理采样函数
     */
    struct ShaderModule
    {
        std::string name;                        ///<模块名称
        std::string file_path;                   ///<模块文件路径
        std::string code;                        ///<GLSL代码内容
        
        std::vector<std::string> provides;                ///<提供的函数名列表（此模块导出的函数）
        std::vector<std::string> requires_funcs;          ///<依赖的函数名列表（需要其他模块提供）
        std::vector<std::string> requires_inputs;         ///<需要的Input变量（如Input.Normal）
        std::vector<std::string> requires_descriptors;    ///<需要的UBO/Sampler（如camera, sky）
        std::vector<std::string> dependencies;            ///<依赖的其他模块名称
    };
    
    /**
     * Shader配方结构
     * 描述如何组合多个模块来生成一个完整的shader
     */
    struct ShaderRecipe
    {
        std::string name;                        ///<配方名称（如"Metal", "Wood"）
        std::string template_file;               ///<使用的模板文件名
        
        std::unordered_map<std::string, std::string> module_map; ///<模块映射："lighting" -> "blinn_phong"
        json template_data;                     ///<传递给inja的模板数据
    };
    
    /**
     * Shader模板引擎
     * 负责加载模块、解析依赖、生成shader代码
     */
    class ShaderTemplateEngine
    {
        inja::Environment env;                  ///<inja模板引擎环境
        
        std::string template_root;               ///<模板文件根目录
        std::string module_root;                 ///<模块文件根目录
        
        std::unordered_map<std::string, std::unique_ptr<ShaderModule>> module_cache;    ///<模块缓存
        std::unordered_map<std::string, json> interface_cache;          ///<接口定义缓存
        
    public:
        /**
         * 构造函数
         * @param template_path 模板文件根路径
         * @param module_path 模块文件根路径
         */
        ShaderTemplateEngine(const std::string& template_path, const std::string& module_path);
        ~ShaderTemplateEngine() = default;
        
        /**
         * 加载指定的模块
         * @param category 模块类别（如"lighting", "specular"）
         * @param name 模块名称（如"blinn_phong"）
         * @return 加载的模块指针，失败返回nullptr
         */
        ShaderModule* LoadModule(const std::string& category, const std::string& name);
        
        /**
         * 解析依赖树并进行拓扑排序
         * @param recipe 要处理的配方
         * @param ordered_modules 输出：排序后的模块名称列表
         * @param missing_deps 输出：缺失的依赖列表
         * @return 成功返回true，存在循环依赖或缺失依赖返回false
         */
        bool ResolveDependencies(const ShaderRecipe& recipe, 
                                std::vector<std::string>& ordered_modules,
                                std::vector<std::string>& missing_deps);
        
        /**
         * 根据配方生成最终的shader代码
         * @param recipe 材质配方
         * @return 生成的GLSL代码
         */
        std::string Generate(const ShaderRecipe& recipe);
        
        /**
         * 从JSON配置文件加载配方
         * @param recipe_file 配方文件路径
         * @param quality_level 质量等级（如"high", "medium", "low"）
         * @return 加载的配方
         */
        ShaderRecipe LoadRecipe(const std::string& recipe_file, const std::string& quality_level);
        
    private:
        /**
         * 构建传递给模板的数据对象
         * @param recipe 配方
         * @param ordered_modules 排序后的模块列表
         * @return inja使用的json数据
         */
        json BuildTemplateData(const ShaderRecipe& recipe, const std::vector<std::string>& ordered_modules);
        
        /**
         * 加载指定类别的接口定义
         * @param category 模块类别
         * @return 成功返回true
         */
        bool LoadInterface(const std::string& category);
        
        /**
         * 读取文件内容到字符串
         * @param file_path 文件路径
         * @return 文件内容，失败返回空字符串
         */
        std::string ReadFileToString(const std::string& file_path);
    };
}//namespace hgl::graph

#endif//HGL_SHADER_TEMPLATE_ENGINE_INCLUDE
