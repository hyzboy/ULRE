/// ShaderTemplateEngine.cpp — Shader 模板渲染引擎实现

#include <hgl/shadergen/ShaderTemplateEngine.h>
#include <hgl/shadergen/FixedMaterialDef.h>
#include <hgl/log/Log.h>
#include <string>

namespace hgl::graph::mtl {

/// 内部数据结构
struct ShaderTemplate {
    std::string name;
    std::string source;  // 原始模板文本
};

struct ShaderRecipe {
    std::string name;
    // TODO: 解析 JSON 后的结构体字段
};

ShaderTemplateEngine::ShaderTemplateEngine()
{
}

ShaderTemplateEngine::~ShaderTemplateEngine()
{
    Reset();
}

std::string ShaderTemplateEngine::ReadFile(const std::string &path)
{
    // TODO: 实现文件读取
    // 返回空串（暂时占位符）
    return std::string();
}

ShaderTemplate *ShaderTemplateEngine::ParseTemplate(const std::string &source)
{
    if (source.empty())
        return nullptr;

    ShaderTemplate *tmpl = new ShaderTemplate();
    tmpl->source = source;
    return tmpl;
}

ShaderRecipe *ShaderTemplateEngine::ParseRecipe(const std::string &json_source)
{
    if (json_source.empty())
        return nullptr;

    ShaderRecipe *recipe = new ShaderRecipe();
    // TODO: 解析 JSON
    return recipe;
}

ShaderTemplate *ShaderTemplateEngine::LoadTemplate(const std::string &template_path)
{

    // 先查缓存
    if (template_cache.contains(template_path))
    {
        auto iter = template_cache.find(template_path);
        if (iter != template_cache.end() && iter->second)
            return iter->second;
    }

    // 从文件读取
    std::string source = ReadFile(template_path);
    if (source.empty())
        return nullptr;

    // 解析
    ShaderTemplate *tmpl = ParseTemplate(source);
    if (!tmpl)
        return nullptr;

    tmpl->name = template_path;
    template_cache[template_path] = tmpl;
    return tmpl;
}

ShaderRecipe *ShaderTemplateEngine::LoadRecipe(const std::string &recipe_path)
{

    // 先查缓存
    if (recipe_cache.contains(recipe_path))
    {
        auto iter = recipe_cache.find(recipe_path);
        if (iter != recipe_cache.end() && iter->second)
            return iter->second;
    }

    // 从文件读取
    std::string source = ReadFile(recipe_path);
    if (source.empty())
        return nullptr;

    // 解析 JSON
    ShaderRecipe *recipe = ParseRecipe(source);
    if (!recipe)
        return nullptr;

    recipe->name = recipe_path;
    recipe_cache[recipe_path] = recipe;
    return recipe;
}

std::string ShaderTemplateEngine::Render(const ShaderTemplate *tmpl, const ShaderRecipe *recipe,
                                        const ShaderPermutationKey &key)
{
    if (!tmpl)
        return std::string();

    // TODO: 调用 inja 渲染
    // 当前的占位符实现只返回排列宏前缀 + 模板源码（不经过 inja 处理）

    std::string result;
    key.AppendGLSLDefines(result);
    result += tmpl->source;

    return result;
}

void ShaderTemplateEngine::Reset()
{
    for (auto [path, tmpl] : template_cache)
        if (tmpl)
            delete tmpl;
    template_cache.clear();

    for (auto [path, recipe] : recipe_cache)
        if (recipe)
            delete recipe;
    recipe_cache.clear();
}

}  // namespace hgl::graph::mtl
