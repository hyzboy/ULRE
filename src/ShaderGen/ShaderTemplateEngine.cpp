#include<hgl/shadergen/ShaderTemplateEngine.h>
#include<algorithm>
#include<set>
#include<queue>
#include<fstream>
#include<sstream>
#include<iostream>

namespace hgl::graph
{
    ShaderTemplateEngine::ShaderTemplateEngine(const std::string& template_path, const std::string& module_path)
        : template_root(template_path), module_root(module_path)
    {
        // 配置inja环境
        env.set_trim_blocks(true);
        env.set_lstrip_blocks(true);
    }
    
    std::string ShaderTemplateEngine::ReadFileToString(const std::string& file_path)
    {
        std::ifstream file(file_path, std::ios::binary);
        if (!file.is_open())
        {
            return "";
        }
        
        std::ostringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }

    ShaderModule* ShaderTemplateEngine::LoadModule(const std::string& category, const std::string& name)
    {
        // 构建缓存键
        std::string cache_key = category + "/" + name;
        
        // 检查缓存
        auto it = module_cache.find(cache_key);
        if(it != module_cache.end())
            return it->second.get();
        
        // 加载接口定义（如果还没加载）
        if(interface_cache.find(category) == interface_cache.end())
        {
            if(!LoadInterface(category))
            {
                std::cerr << "Failed to load interface for category: " << category << std::endl;
                return nullptr;
            }
        }
        
        // 从接口定义中获取模块信息
        const json& interface = interface_cache[category];
        if(!interface.contains("modules") || !interface["modules"].contains(name))
        {
            std::cerr << "Module not found in interface: " << category << "/" << name << std::endl;
            return nullptr;
        }
        
        const json& module_def = interface["modules"][name];
        
        // 创建模块对象
        auto module = std::make_unique<ShaderModule>();
        module->name = name;
        
        // 读取GLSL代码文件
        std::string file_path = module_root + "/" + category + "/" + module_def["file"].get<std::string>();
        module->file_path = file_path;
        
        std::string code = ReadFileToString(file_path);
        if(code.empty())
        {
            std::cerr << "Failed to load module file: " << file_path << std::endl;
            return nullptr;
        }
        
        module->code = code;
        
        // 解析provides
        if(module_def.contains("provides"))
        {
            for(const auto& func : module_def["provides"])
            {
                module->provides.push_back(func.get<std::string>());
            }
        }
        
        // 解析requires
        if(module_def.contains("requires"))
        {
            const json& requires = module_def["requires"];
            
            if(requires.contains("functions"))
            {
                for(const auto& func : requires["functions"])
                {
                    module->requires_funcs.push_back(func.get<std::string>());
                }
            }
            
            if(requires.contains("inputs"))
            {
                for(const auto& input : requires["inputs"])
                {
                    module->requires_inputs.push_back(input.get<std::string>());
                }
            }
            
            if(requires.contains("descriptors"))
            {
                for(const auto& desc : requires["descriptors"])
                {
                    module->requires_descriptors.push_back(desc.get<std::string>());
                }
            }
        }
        
        // 解析dependencies
        if(module_def.contains("dependencies"))
        {
            for(const auto& dep : module_def["dependencies"])
            {
                module->dependencies.push_back(dep.get<std::string>());
            }
        }
        
        // 缓存模块
        ShaderModule* module_ptr = module.get();
        module_cache[cache_key] = std::move(module);
        
        return module_ptr;
    }

    bool ShaderTemplateEngine::LoadInterface(const std::string& category)
    {
        std::string interface_file = module_root + "/" + category + "/" + category + "_interface.json";
        
        std::string json_str = ReadFileToString(interface_file);
        if(json_str.empty())
        {
            std::cerr << "Failed to load interface file: " << interface_file << std::endl;
            return false;
        }
        
        try
        {
            json interface_json = json::parse(json_str);
            interface_cache[category] = interface_json;
            return true;
        }
        catch(const json::exception& e)
        {
            std::cerr << "Failed to parse interface JSON: " << interface_file << ", error: " << e.what() << std::endl;
            return false;
        }
    }

    bool ShaderTemplateEngine::ResolveDependencies(const ShaderRecipe& recipe, 
                                                   std::vector<std::string>& ordered_modules,
                                                   std::vector<std::string>& missing_deps)
    {
        ordered_modules.clear();
        missing_deps.clear();
        
        // 收集所有需要的模块
        std::map<std::string, ShaderModule*> all_modules;
        std::set<std::string> to_process;
        
        // 从配方中的模块开始
        for(const auto& [category, module_name] : recipe.module_map)
        {
            ShaderModule* module = LoadModule(category, module_name);
            if(!module)
            {
                missing_deps.push_back(category + "/" + module_name);
                continue;
            }
            
            std::string key = category + "/" + module_name;
            all_modules[key] = module;
            to_process.insert(key);
        }
        
        // 递归处理依赖
        while(!to_process.empty())
        {
            std::string current_key = *to_process.begin();
            to_process.erase(to_process.begin());
            
            ShaderModule* current = all_modules[current_key];
            
            // 处理此模块的依赖
            for(const auto& dep_func : current->dependencies)
            {
                // 查找提供此函数的模块
                bool found = false;
                for(const auto& [key, module] : all_modules)
                {
                    for(const auto& provided : module->provides)
                    {
                        if(provided == dep_func)
                        {
                            found = true;
                            break;
                        }
                    }
                    if(found) break;
                }
                
                if(!found)
                {
                    // 需要加载提供此函数的模块 - 这里简化处理，假设依赖直接是模块名
                    // 在实际实现中，可能需要更复杂的查找逻辑
                    std::cerr << "Warning: Dependency function not found: " << dep_func << std::endl;
                }
            }
        }
        
        // 拓扑排序
        std::map<std::string, int> in_degree;
        std::map<std::string, std::vector<std::string>> graph;
        
        // 初始化入度
        for(const auto& [key, module] : all_modules)
        {
            in_degree[key] = 0;
            graph[key] = std::vector<std::string>();
        }
        
        // 构建依赖图（注意：依赖是反向的）
        for(const auto& [key, module] : all_modules)
        {
            // 对于每个此模块提供的函数，找到需要它的模块
            for(const auto& provided_func : module->provides)
            {
                for(const auto& [other_key, other] : all_modules)
                {
                    if(other_key == key) continue;
                    
                    for(const auto& dep : other->dependencies)
                    {
                        if(dep == provided_func)
                        {
                            // other依赖于module，所以module必须在other之前
                            graph[key].push_back(other_key);
                            in_degree[other_key]++;
                        }
                    }
                }
            }
        }
        
        // Kahn算法进行拓扑排序
        std::queue<std::string> zero_in_degree;
        for(const auto& [key, degree] : in_degree)
        {
            if(degree == 0)
                zero_in_degree.push(key);
        }
        
        while(!zero_in_degree.empty())
        {
            std::string current = zero_in_degree.front();
            zero_in_degree.pop();
            
            ordered_modules.push_back(current);
            
            for(const std::string& neighbor : graph[current])
            {
                in_degree[neighbor]--;
                if(in_degree[neighbor] == 0)
                    zero_in_degree.push(neighbor);
            }
        }
        
        // 检查是否有循环依赖
        if(ordered_modules.size() != all_modules.size())
        {
            std::cerr << "Error: Circular dependency detected" << std::endl;
            return false;
        }
        
        return missing_deps.empty();
    }

    json ShaderTemplateEngine::BuildTemplateData(const ShaderRecipe& recipe, const std::vector<std::string>& ordered_modules)
    {
        json data = recipe.template_data;
        
        // 收集所有模块的代码
        std::string all_module_code;
        for(const auto& module_key : ordered_modules)
        {
            auto it = module_cache.find(module_key);
            if(it != module_cache.end())
            {
                ShaderModule* module = it->second.get();
                all_module_code += "\n// ========== Module: " + module_key + " ==========\n";
                all_module_code += module->code;
                all_module_code += "\n";
            }
        }
        
        data["module_functions"] = all_module_code;
        
        return data;
    }

    std::string ShaderTemplateEngine::Generate(const ShaderRecipe& recipe)
    {
        // 解析依赖
        std::vector<std::string> ordered_modules;
        std::vector<std::string> missing_deps;
        
        if(!ResolveDependencies(recipe, ordered_modules, missing_deps))
        {
            std::cerr << "Error: Failed to resolve dependencies" << std::endl;
            for(const auto& dep : missing_deps)
            {
                std::cerr << "  Missing: " << dep << std::endl;
            }
            return "";
        }
        
        // 构建模板数据
        json template_data = BuildTemplateData(recipe, ordered_modules);
        
        // 加载模板文件
        std::string template_path = template_root + "/" + recipe.template_file;
        
        std::string template_str = ReadFileToString(template_path);
        if(template_str.empty())
        {
            std::cerr << "Error: Failed to load template file: " << template_path << std::endl;
            return "";
        }
        
        // 渲染模板
        try
        {
            std::string result = env.render(template_str, template_data);
            return result;
        }
        catch(const std::exception& e)
        {
            std::cerr << "Error: Template rendering failed: " << e.what() << std::endl;
            return "";
        }
    }

    ShaderRecipe ShaderTemplateEngine::LoadRecipe(const std::string& recipe_file, const std::string& quality_level)
    {
        ShaderRecipe recipe;
        
        // 加载配方文件
        std::string json_str = ReadFileToString(recipe_file);
        if(json_str.empty())
        {
            std::cerr << "Error: Failed to load recipe file: " << recipe_file << std::endl;
            return recipe;
        }
        
        try
        {
            json recipe_json = json::parse(json_str);
            
            recipe.name = recipe_json["name"].get<std::string>();
            
            // 获取指定质量等级的配置
            if(!recipe_json.contains("quality_levels") || 
               !recipe_json["quality_levels"].contains(quality_level))
            {
                std::cerr << "Error: Quality level not found: " << quality_level << std::endl;
                return recipe;
            }
            
            const json& quality_config = recipe_json["quality_levels"][quality_level];
            
            // 解析fragment shader配置
            if(quality_config.contains("fragment_shader"))
            {
                const json& frag_config = quality_config["fragment_shader"];
                
                recipe.template_file = frag_config["template"].get<std::string>();
                
                // 解析模块映射
                if(frag_config.contains("modules"))
                {
                    for(auto it = frag_config["modules"].begin(); it != frag_config["modules"].end(); ++it)
                    {
                        recipe.module_map[it.key()] = it.value().get<std::string>();
                    }
                }
                
                // 保存模板数据
                recipe.template_data = frag_config;
            }
        }
        catch(const json::exception& e)
        {
            std::cerr << "Error: Failed to parse recipe JSON: " << e.what() << std::endl;
        }
        
        return recipe;
    }
}//namespace hgl::graph
