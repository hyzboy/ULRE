#include <hgl/graph/module/ShaderGenVertexInputAdapter.h>
#include <hgl/vk/VKRenderAssign.h>
#include <cstdio>

using namespace hgl::graph;

int main()
{
    int failed = 0;

    if (ResolveVertexInputGroupBySemantic(Assign::TransformID::VIS_NAME) != VertexInputGroup::TransformID)
    {
        std::fprintf(stderr, "[FAIL] Resolve group for TransformID\n");
        ++failed;
    }

    if (ResolveVertexInputGroupBySemantic(Assign::MaterialInstanceID::VIS_NAME) != VertexInputGroup::MaterialInstanceID)
    {
        std::fprintf(stderr, "[FAIL] Resolve group for MaterialInstanceID\n");
        ++failed;
    }

    if (ResolveVertexInputGroupBySemantic(VAN::Position) != VertexInputGroup::Basic)
    {
        std::fprintf(stderr, "[FAIL] Resolve group for Position\n");
        ++failed;
    }

    mtl::contract::VertexInputLayout layout;
    layout.attributes.push_back({0, VAN::Position, "vec2", uint32_t(VK_VERTEX_INPUT_RATE_VERTEX)});
    layout.attributes.push_back({1, Assign::TransformID::VIS_NAME, "uint", uint32_t(VK_VERTEX_INPUT_RATE_INSTANCE)});
    layout.attributes.push_back({2, Assign::MaterialInstanceID::VIS_NAME, "uint", uint32_t(VK_VERTEX_INPUT_RATE_INSTANCE)});

    VIAArray input;
    std::string reason;
    if (!BuildVertexInputFromContractLayout(layout, input, reason))
    {
        std::fprintf(stderr, "[FAIL] BuildVertexInputFromContractLayout failed: %s\n", reason.c_str());
        ++failed;
    }
    else
    {
        if (input.count != 3)
        {
            std::fprintf(stderr, "[FAIL] input.count mismatch (%u != 3)\n", input.count);
            ++failed;
        }
        else
        {
            if (input.items[0].group != VertexInputGroup::Basic)
            {
                std::fprintf(stderr, "[FAIL] Position group mismatch\n");
                ++failed;
            }

            if (input.items[1].group != VertexInputGroup::TransformID)
            {
                std::fprintf(stderr, "[FAIL] TransformID group mismatch\n");
                ++failed;
            }

            if (input.items[2].group != VertexInputGroup::MaterialInstanceID)
            {
                std::fprintf(stderr, "[FAIL] MaterialInstanceID group mismatch\n");
                ++failed;
            }
        }
    }

    mtl::contract::VertexInputLayout duplicate_location_layout;
    duplicate_location_layout.attributes.push_back({0, VAN::Position, "vec2", uint32_t(VK_VERTEX_INPUT_RATE_VERTEX)});
    duplicate_location_layout.attributes.push_back({0, VAN::Color, "vec4", uint32_t(VK_VERTEX_INPUT_RATE_VERTEX)});

    VIAArray duplicate_input;
    reason.clear();
    if (BuildVertexInputFromContractLayout(duplicate_location_layout, duplicate_input, reason))
    {
        std::fprintf(stderr, "[FAIL] duplicate location should fail\n");
        ++failed;
    }

    if (failed != 0)
    {
        std::fprintf(stderr, "ShaderGenVertexInputAdapterTest FAILED (%d)\n", failed);
        return 1;
    }

    std::fprintf(stdout, "ShaderGenVertexInputAdapterTest PASSED\n");
    return 0;
}
