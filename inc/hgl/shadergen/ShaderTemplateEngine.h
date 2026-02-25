/// ShaderTemplateEngine.h — Shader 模板渲染引擎（基于 inja）
///
/// 功能：
///   - 加载 ShaderLibrary/ 中的模板文件（.tmpl、.glsl）
///   - 解析 recipe JSON（定义排列组合）
///   - 调用 inja 渲染模板 → 生成 GLSL 代码

#pragma once

#include <string>
#include <unordered_map>



namespace hgl::graph::mtl {

// 前向声明
struct ShaderPermutationKey;
struct ShaderTemplate;
struct ShaderRecipe;

/**
 * ShaderTemplateEngine — Shader 生成模板系统
 *
 * 当前版本（M0）实现基本框架和 inja 集成，实际渲染逻辑在 M2-M3 补全。
 * 
 * 使用流程：
 *   1. engine.LoadTemplate(path) → 读取 .tmpl 或 .glsl 文件
 *   2. engine.LoadRecipe(path)   → 读取 recipe JSON 定义排列
 *   3. engine.Render(recipe, permutation_key) → 渲染 GLSL
 */
class ShaderTemplateEngine
{
public:
    ShaderTemplateEngine();
    ~ShaderTemplateEngine();

    /// 从文件加载策咨询模板
    /// @param template_path 相对 ShaderLibrary/ 的路径，如 "templates/forward_uber.frag.tmpl"
    /// @return 成功返回 template，失败返回 nullptr
    ShaderTemplate *LoadTemplate(const std::string &template_path);

    /// 从文件加载 recipe（排列定义）
    /// @param recipe_path 相对 ShaderLibrary/ 的路径，如 "recipes/uber/uber_3d.json"
    /// @return 成功返回 recipe，失败返回 nullptr
    ShaderRecipe *LoadRecipe(const std::string &recipe_path);

    /// 用排列 key 渲染模板 → GLSL 源码
    /// @param tmpl 模板对象（由 LoadTemplate 返回）
    /// @param recipe recipe 对象（由 LoadRecipe 返回）
    /// @param key 排列键（宏定义源）
    /// @return 渲染后的 GLSL 源码；失败返回空串
    std::string Render(const ShaderTemplate *tmpl, const ShaderRecipe *recipe, 
                     const ShaderPermutationKey &key);

    /// 清空所有缓存
    void Reset();

private:
    std::unordered_map<std::string, ShaderTemplate *> template_cache;
    std::unordered_map<std::string, ShaderRecipe *> recipe_cache;

    // 辅助方法
    std::string ReadFile(const std::string &path);
    ShaderTemplate *ParseTemplate(const std::string &source);
    ShaderRecipe *ParseRecipe(const std::string &json_source);
};

}  // namespace hgl::graph::mtl
