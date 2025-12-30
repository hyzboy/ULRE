// Shader Template Engine Test Example
// This example demonstrates basic usage of the ShaderTemplateEngine

#include<hgl/shadergen/ShaderTemplateEngine.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/log/Logger.h>
#include<iostream>

using namespace hgl::graph;

void PrintSeparator()
{
    std::cout << "\n" << std::string(80, '=') << "\n\n";
}

bool TestModuleLoading(ShaderTemplateEngine& engine)
{
    std::cout << "TEST 1: Module Loading\n";
    std::cout << "-----------------------\n";
    
    // Test loading a simple module
    ShaderModule* lambert = engine.LoadModule(
        AnsiString("lighting"),
        AnsiString("lambert")
    );
    
    if (!lambert)
    {
        std::cout << "FAILED: Could not load lambert module\n";
        return false;
    }
    
    std::cout << "SUCCESS: Loaded lambert module\n";
    std::cout << "  - Name: " << lambert->name.c_str() << "\n";
    std::cout << "  - Provides: ";
    for (int i = 0; i < lambert->provides.GetCount(); ++i)
    {
        std::cout << lambert->provides[i].c_str();
        if (i < lambert->provides.GetCount() - 1) std::cout << ", ";
    }
    std::cout << "\n";
    std::cout << "  - Dependencies: ";
    for (int i = 0; i < lambert->dependencies.GetCount(); ++i)
    {
        std::cout << lambert->dependencies[i].c_str();
        if (i < lambert->dependencies.GetCount() - 1) std::cout << ", ";
    }
    std::cout << "\n";
    
    // Test loading another module
    ShaderModule* textureAlbedo = engine.LoadModule(
        AnsiString("albedo"),
        AnsiString("texture_albedo")
    );
    
    if (!textureAlbedo)
    {
        std::cout << "FAILED: Could not load texture_albedo module\n";
        return false;
    }
    
    std::cout << "SUCCESS: Loaded texture_albedo module\n";
    
    return true;
}

bool TestDependencyResolution(ShaderTemplateEngine& engine)
{
    std::cout << "TEST 2: Dependency Resolution\n";
    std::cout << "------------------------------\n";
    
    // Create a simple recipe
    ShaderRecipe recipe;
    recipe.name = AnsiString("TestRecipe");
    recipe.template_file = AnsiString("forward.frag.tmpl");
    
    // Add some modules
    recipe.module_map.Add(AnsiString("lighting"), AnsiString("lambert"));
    recipe.module_map.Add(AnsiString("albedo"), AnsiString("texture_albedo"));
    recipe.module_map.Add(AnsiString("specular"), AnsiString("phong"));
    recipe.module_map.Add(AnsiString("ambient"), AnsiString("skylight"));
    
    // Resolve dependencies
    AnsiStringList orderedModules;
    AnsiStringList missingDeps;
    
    bool success = engine.ResolveDependencies(recipe, orderedModules, missingDeps);
    
    if (!success || missingDeps.GetCount() > 0)
    {
        std::cout << "FAILED: Dependency resolution failed\n";
        std::cout << "Missing dependencies:\n";
        for (int i = 0; i < missingDeps.GetCount(); ++i)
        {
            std::cout << "  - " << missingDeps[i].c_str() << "\n";
        }
        return false;
    }
    
    std::cout << "SUCCESS: Dependencies resolved\n";
    std::cout << "Module load order:\n";
    for (int i = 0; i < orderedModules.GetCount(); ++i)
    {
        std::cout << "  " << (i + 1) << ". " << orderedModules[i].c_str() << "\n";
    }
    
    return true;
}

bool TestRecipeLoading(ShaderTemplateEngine& engine, const AnsiString& recipePath)
{
    std::cout << "TEST 3: Recipe Loading\n";
    std::cout << "----------------------\n";
    
    ShaderRecipe recipe = engine.LoadRecipe(recipePath, AnsiString("high"));
    
    if (recipe.name.IsEmpty())
    {
        std::cout << "FAILED: Could not load recipe from " << recipePath.c_str() << "\n";
        return false;
    }
    
    std::cout << "SUCCESS: Loaded recipe\n";
    std::cout << "  - Name: " << recipe.name.c_str() << "\n";
    std::cout << "  - Template: " << recipe.template_file.c_str() << "\n";
    std::cout << "  - Modules:\n";
    
    for (auto it = recipe.module_map.begin(); it != recipe.module_map.end(); ++it)
    {
        std::cout << "      " << it->key.c_str() << " -> " 
                  << it->value.c_str() << "\n";
    }
    
    return true;
}

bool TestShaderGeneration(ShaderTemplateEngine& engine, const AnsiString& recipePath)
{
    std::cout << "TEST 4: Shader Code Generation\n";
    std::cout << "-------------------------------\n";
    
    ShaderRecipe recipe = engine.LoadRecipe(recipePath, AnsiString("low"));
    
    if (recipe.name.IsEmpty())
    {
        std::cout << "FAILED: Could not load recipe\n";
        return false;
    }
    
    AnsiString shaderCode = engine.Generate(recipe);
    
    if (shaderCode.IsEmpty())
    {
        std::cout << "FAILED: Generated shader code is empty\n";
        return false;
    }
    
    std::cout << "SUCCESS: Generated shader code (" 
              << shaderCode.Length() << " bytes)\n";
    std::cout << "\nGenerated GLSL (first 500 chars):\n";
    std::cout << std::string(40, '-') << "\n";
    
    const char* codeStr = shaderCode.c_str();
    int length = shaderCode.Length();
    int displayLength = (length < 500) ? length : 500;
    
    std::cout << std::string(codeStr, displayLength);
    if (length > 500)
    {
        std::cout << "\n... (" << (length - 500) << " more bytes)";
    }
    std::cout << "\n";
    
    return true;
}

int main(int argc, char** argv)
{
    std::cout << "Shader Template Engine Test Suite\n";
    std::cout << "==================================\n\n";
    
    // Determine paths
    AnsiString baseDir;
    if (argc > 1)
    {
        baseDir = AnsiString(argv[1]);
    }
    else
    {
        // Try to find the ShaderLibrary directory
        baseDir = AnsiString("./");
    }
    
    AnsiString templatePath = baseDir + AnsiString("ShaderLibrary/templates");
    AnsiString modulePath = baseDir + AnsiString("ShaderLibrary/modules");
    AnsiString recipePath = baseDir + AnsiString("ShaderLibrary/recipes/standard/metal.json");
    
    std::cout << "Configuration:\n";
    std::cout << "  - Templates: " << templatePath.c_str() << "\n";
    std::cout << "  - Modules: " << modulePath.c_str() << "\n";
    std::cout << "  - Recipe: " << recipePath.c_str() << "\n";
    
    PrintSeparator();
    
    try
    {
        // Initialize engine
        ShaderTemplateEngine engine(templatePath, modulePath);
        
        // Run tests
        bool allPassed = true;
        
        if (!TestModuleLoading(engine))
        {
            allPassed = false;
        }
        PrintSeparator();
        
        if (!TestDependencyResolution(engine))
        {
            allPassed = false;
        }
        PrintSeparator();
        
        if (!TestRecipeLoading(engine, recipePath))
        {
            allPassed = false;
        }
        PrintSeparator();
        
        if (!TestShaderGeneration(engine, recipePath))
        {
            allPassed = false;
        }
        PrintSeparator();
        
        // Summary
        std::cout << "Test Summary\n";
        std::cout << "============\n";
        if (allPassed)
        {
            std::cout << "ALL TESTS PASSED ✓\n";
            return 0;
        }
        else
        {
            std::cout << "SOME TESTS FAILED ✗\n";
            return 1;
        }
    }
    catch (const std::exception& e)
    {
        std::cout << "EXCEPTION: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
