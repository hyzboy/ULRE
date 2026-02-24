/// ShaderTemplateEngine.cpp — Shader 模板渲染引擎实现

#include <hgl/shadergen/ShaderTemplateEngine.h>
#include <hgl/graph/mtl/FixedMaterialDef.h>
#include <hgl/log/Log.h>

namespace hgl::graph::mtl {

/// 内部数据结构
struct ShaderTemplate {
    AnsiString name;
    AnsiString source;  // 原始模板文本
};

struct ShaderRecipe {
    AnsiString name;
    // TODO: 解析 JSON 后的结构体字段
};

ShaderTemplateEngine::ShaderTemplateEngine()
{
}

ShaderTemplateEngine::~ShaderTemplateEngine()
{
    Reset();
}

AnsiString ShaderTemplateEngine::ReadFile(const AnsiString &path)
{
    // TODO: 实现文件读取
    // 返回空串（暂时占位符）
    return AnsiString();
}

ShaderTemplate *ShaderTemplateEngine::ParseTemplate(const AnsiString &source)
{
    if (source.IsEmpty())
        return nullptr;

    ShaderTemplate *tmpl = new ShaderTemplate();
    tmpl->source = source;
    return tmpl;
}

ShaderRecipe *ShaderTemplateEngine::ParseRecipe(const AnsiString &json_source)
{
    if (json_source.IsEmpty())
        return nullptr;

    ShaderRecipe *recipe = new ShaderRecipe();
    // TODO: 解析 JSON
    return recipe;
}

ShaderTemplate *ShaderTemplateEngine::LoadTemplate(const AnsiString &template_path)
{

    // 先查缓存
    if (template_cache.ContainsKey(template_path))
    {
        ShaderTemplate **cached = template_cache.GetValuePointer(template_path);
        if (cached && *cached)
            return *cached;
    }

    // 从文件读取
    AnsiString source = ReadFile(template_path);
    if (source.IsEmpty())
        return nullptr;

    // 解析
    ShaderTemplate *tmpl = ParseTemplate(source);
    if (!tmpl)
        return nullptr;

    tmpl->name = template_path;
    template_cache[template_path] = tmpl;
    return tmpl;
}

ShaderRecipe *ShaderTemplateEngine::LoadRecipe(const AnsiString &recipe_path)
{

    // 先查缓存
    if (recipe_cache.ContainsKey(recipe_path))
    {
        ShaderRecipe **cached = recipe_cache.GetValuePointer(recipe_path);
        if (cached && *cached)
            return *cached;
    }

    // 从文件读取
    AnsiString source = ReadFile(recipe_path);
    if (source.IsEmpty())
        return nullptr;

    // 解析 JSON
    ShaderRecipe *recipe = ParseRecipe(source);
    if (!recipe)
        return nullptr;

    recipe->name = recipe_path;
    recipe_cache[recipe_path] = recipe;
    return recipe;
}

AnsiString ShaderTemplateEngine::Render(const ShaderTemplate *tmpl, const ShaderRecipe *recipe,
                                        const ShaderPermutationKey &key)
{
    if (!tmpl)
        return AnsiString();

    // TODO: 调用 inja 渲染
    // 当前的占位符实现只返回排列宏前缀 + 模板源码（不经过 inja 处理）

    AnsiString result;
    key.AppendGLSLDefines(result);
    result += tmpl->source;

    return result;
}

void ShaderTemplateEngine::Reset()
{
    for (auto [path, tmpl] : template_cache)
        if (tmpl)
            delete tmpl;
    template_cache.Clear();

    for (auto [path, recipe] : recipe_cache)
        if (recipe)
            delete recipe;
    recipe_cache.Clear();
}

}  // namespace hgl::graph::mtl
