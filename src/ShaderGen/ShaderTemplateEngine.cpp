#include<hgl/shadergen/ShaderTemplateEngine.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/io/FileAccess.h>
#include<hgl/log/Logger.h>
#include<algorithm>
#include<set>
#include<queue>

namespace hgl::graph
{
    ShaderTemplateEngine::ShaderTemplateEngine(const AnsiString& template_path, const AnsiString& module_path)
        : template_root(template_path), module_root(module_path)
    {
        // 配置inja环境
        env.set_trim_blocks(true);
        env.set_lstrip_blocks(true);
    }

    ShaderTemplateEngine::~ShaderTemplateEngine()
    {
        // 清理缓存的模块
        for(auto& pair : module_cache)
        {
            delete pair.value;
        }
        module_cache.Clear();
    }

    ShaderModule* ShaderTemplateEngine::LoadModule(const AnsiString& category, const AnsiString& name)
    {
        // 构建缓存键
        AnsiString cache_key = category + "/" + name;
        
        // 检查缓存
        if(module_cache.ContainsKey(cache_key))
            return module_cache[cache_key];
        
        // 加载接口定义（如果还没加载）
        if(!interface_cache.ContainsKey(category))
        {
            if(!LoadInterface(category))
            {
                LOG_ERROR("Failed to load interface for category: " + category);
                return nullptr;
            }
        }
        
        // 从接口定义中获取模块信息
        const json& interface = interface_cache[category];
        if(!interface.contains("modules") || !interface["modules"].contains(name.c_str()))
        {
            LOG_ERROR("Module not found in interface: " + category + "/" + name);
            return nullptr;
        }
        
        const json& module_def = interface["modules"][name.c_str()];
        
        // 创建模块对象
        ShaderModule* module = new ShaderModule();
        module->name = name;
        
        // 读取GLSL代码文件
        AnsiString file_path = module_root + "/" + category + "/" + AnsiString(module_def["file"].get<std::string>().c_str());
        module->file_path = file_path;
        
        void* fp = filesystem::LoadFileToMemory(file_path);
        if(!fp)
        {
            LOG_ERROR("Failed to load module file: " + file_path);
            delete module;
            return nullptr;
        }
        
        const int64 file_size = filesystem::GetFileLength(file_path);
        module->code.Set((char*)fp, file_size);
        hgl_free(fp);
        
        // 解析provides
        if(module_def.contains("provides"))
        {
            for(const auto& func : module_def["provides"])
            {
                module->provides.Add(AnsiString(func.get<std::string>().c_str()));
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
                    module->requires_funcs.Add(AnsiString(func.get<std::string>().c_str()));
                }
            }
            
            if(requires.contains("inputs"))
            {
                for(const auto& input : requires["inputs"])
                {
                    module->requires_inputs.Add(AnsiString(input.get<std::string>().c_str()));
                }
            }
            
            if(requires.contains("descriptors"))
            {
                for(const auto& desc : requires["descriptors"])
                {
                    module->requires_descriptors.Add(AnsiString(desc.get<std::string>().c_str()));
                }
            }
        }
        
        // 解析dependencies
        if(module_def.contains("dependencies"))
        {
            for(const auto& dep : module_def["dependencies"])
            {
                module->dependencies.Add(AnsiString(dep.get<std::string>().c_str()));
            }
        }
        
        // 缓存模块
        module_cache.Add(cache_key, module);
        
        return module;
    }

    bool ShaderTemplateEngine::LoadInterface(const AnsiString& category)
    {
        AnsiString interface_file = module_root + "/" + category + "/" + category + "_interface.json";
        
        void* fp = filesystem::LoadFileToMemory(interface_file);
        if(!fp)
        {
            LOG_ERROR("Failed to load interface file: " + interface_file);
            return false;
        }
        
        const int64 file_size = filesystem::GetFileLength(interface_file);
        std::string json_str((char*)fp, file_size);
        hgl_free(fp);
        
        try
        {
            json interface_json = json::parse(json_str);
            interface_cache.Add(category, interface_json);
            return true;
        }
        catch(const json::exception& e)
        {
            LOG_ERROR("Failed to parse interface JSON: " + interface_file + ", error: " + AnsiString(e.what()));
            return false;
        }
    }

    bool ShaderTemplateEngine::ResolveDependencies(const ShaderRecipe& recipe, 
                                                   AnsiStringList& ordered_modules,
                                                   AnsiStringList& missing_deps)
    {
        ordered_modules.Clear();
        missing_deps.Clear();
        
        // 收集所有需要的模块
        std::map<std::string, ShaderModule*> all_modules;
        std::set<std::string> to_process;
        
        // 从配方中的模块开始
        for(auto it = recipe.module_map.begin(); it != recipe.module_map.end(); ++it)
        {
            const AnsiString& category = it->key;
            const AnsiString& module_name = it->value;
            
            ShaderModule* module = LoadModule(category, module_name);
            if(!module)
            {
                missing_deps.Add(category + "/" + module_name);
                continue;
            }
            
            std::string key = std::string(category.c_str()) + "/" + std::string(module_name.c_str());
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
            for(int i = 0; i < current->dependencies.GetCount(); ++i)
            {
                const AnsiString& dep_func = current->dependencies[i];
                
                // 查找提供此函数的模块
                bool found = false;
                for(const auto& pair : all_modules)
                {
                    for(int j = 0; j < pair.second->provides.GetCount(); ++j)
                    {
                        if(pair.second->provides[j] == dep_func)
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
                    LOG_WARNING("Dependency function not found: " + dep_func);
                }
            }
        }
        
        // 拓扑排序
        std::map<std::string, int> in_degree;
        std::map<std::string, std::vector<std::string>> graph;
        
        // 初始化入度
        for(const auto& pair : all_modules)
        {
            in_degree[pair.first] = 0;
            graph[pair.first] = std::vector<std::string>();
        }
        
        // 构建依赖图（注意：依赖是反向的）
        for(const auto& pair : all_modules)
        {
            const ShaderModule* module = pair.second;
            
            // 对于每个此模块提供的函数，找到需要它的模块
            for(int i = 0; i < module->provides.GetCount(); ++i)
            {
                const AnsiString& provided_func = module->provides[i];
                
                for(const auto& other_pair : all_modules)
                {
                    if(other_pair.first == pair.first) continue;
                    
                    const ShaderModule* other = other_pair.second;
                    for(int j = 0; j < other->dependencies.GetCount(); ++j)
                    {
                        if(other->dependencies[j] == provided_func)
                        {
                            // other依赖于module，所以module必须在other之前
                            graph[pair.first].push_back(other_pair.first);
                            in_degree[other_pair.first]++;
                        }
                    }
                }
            }
        }
        
        // Kahn算法进行拓扑排序
        std::queue<std::string> zero_in_degree;
        for(const auto& pair : in_degree)
        {
            if(pair.second == 0)
                zero_in_degree.push(pair.first);
        }
        
        while(!zero_in_degree.empty())
        {
            std::string current = zero_in_degree.front();
            zero_in_degree.pop();
            
            ordered_modules.Add(AnsiString(current.c_str()));
            
            for(const std::string& neighbor : graph[current])
            {
                in_degree[neighbor]--;
                if(in_degree[neighbor] == 0)
                    zero_in_degree.push(neighbor);
            }
        }
        
        // 检查是否有循环依赖
        if(ordered_modules.GetCount() != all_modules.size())
        {
            LOG_ERROR("Circular dependency detected");
            return false;
        }
        
        return missing_deps.GetCount() == 0;
    }

    json ShaderTemplateEngine::BuildTemplateData(const ShaderRecipe& recipe, const AnsiStringList& ordered_modules)
    {
        json data = recipe.template_data;
        
        // 收集所有模块的代码
        std::string all_module_code;
        for(int i = 0; i < ordered_modules.GetCount(); ++i)
        {
            const AnsiString& module_key = ordered_modules[i];
            
            if(module_cache.ContainsKey(module_key))
            {
                ShaderModule* module = module_cache[module_key];
                all_module_code += "\n// ========== Module: " + std::string(module_key.c_str()) + " ==========\n";
                all_module_code += std::string(module->code.c_str());
                all_module_code += "\n";
            }
        }
        
        data["module_functions"] = all_module_code;
        
        return data;
    }

    AnsiString ShaderTemplateEngine::Generate(const ShaderRecipe& recipe)
    {
        // 解析依赖
        AnsiStringList ordered_modules;
        AnsiStringList missing_deps;
        
        if(!ResolveDependencies(recipe, ordered_modules, missing_deps))
        {
            LOG_ERROR("Failed to resolve dependencies");
            for(int i = 0; i < missing_deps.GetCount(); ++i)
            {
                LOG_ERROR("  Missing: " + missing_deps[i]);
            }
            return AnsiString();
        }
        
        // 构建模板数据
        json template_data = BuildTemplateData(recipe, ordered_modules);
        
        // 加载模板文件
        AnsiString template_path = template_root + "/" + recipe.template_file;
        
        void* fp = filesystem::LoadFileToMemory(template_path);
        if(!fp)
        {
            LOG_ERROR("Failed to load template file: " + template_path);
            return AnsiString();
        }
        
        const int64 file_size = filesystem::GetFileLength(template_path);
        std::string template_str((char*)fp, file_size);
        hgl_free(fp);
        
        // 渲染模板
        try
        {
            std::string result = env.render(template_str, template_data);
            return AnsiString(result.c_str());
        }
        catch(const std::exception& e)
        {
            LOG_ERROR("Template rendering failed: " + AnsiString(e.what()));
            return AnsiString();
        }
    }

    ShaderRecipe ShaderTemplateEngine::LoadRecipe(const AnsiString& recipe_file, const AnsiString& quality_level)
    {
        ShaderRecipe recipe;
        
        // 加载配方文件
        void* fp = filesystem::LoadFileToMemory(recipe_file);
        if(!fp)
        {
            LOG_ERROR("Failed to load recipe file: " + recipe_file);
            return recipe;
        }
        
        const int64 file_size = filesystem::GetFileLength(recipe_file);
        std::string json_str((char*)fp, file_size);
        hgl_free(fp);
        
        try
        {
            json recipe_json = json::parse(json_str);
            
            recipe.name = AnsiString(recipe_json["name"].get<std::string>().c_str());
            
            // 获取指定质量等级的配置
            if(!recipe_json.contains("quality_levels") || 
               !recipe_json["quality_levels"].contains(quality_level.c_str()))
            {
                LOG_ERROR("Quality level not found: " + quality_level);
                return recipe;
            }
            
            const json& quality_config = recipe_json["quality_levels"][quality_level.c_str()];
            
            // 解析fragment shader配置
            if(quality_config.contains("fragment_shader"))
            {
                const json& frag_config = quality_config["fragment_shader"];
                
                recipe.template_file = AnsiString(frag_config["template"].get<std::string>().c_str());
                
                // 解析模块映射
                if(frag_config.contains("modules"))
                {
                    for(auto it = frag_config["modules"].begin(); it != frag_config["modules"].end(); ++it)
                    {
                        recipe.module_map.Add(
                            AnsiString(it.key().c_str()),
                            AnsiString(it.value().get<std::string>().c_str())
                        );
                    }
                }
                
                // 保存模板数据
                recipe.template_data = frag_config;
            }
        }
        catch(const json::exception& e)
        {
            LOG_ERROR("Failed to parse recipe JSON: " + AnsiString(e.what()));
        }
        
        return recipe;
    }
}//namespace hgl::graph
