#include<hgl/ecs/MaterialBatch.h>
#include<hgl/ecs/RenderItem.h>
#include<algorithm>

namespace hgl::ecs
{
    void MaterialBatch::AddItem(RenderItem* item)
    {
        if (item)
            items.push_back(item);
    }

    void MaterialBatch::Finalize()
    {
        // Sort items by geometry/distance for optimal rendering
        std::sort(items.begin(), items.end(),
            [](const RenderItem* a, const RenderItem* b) {
                return a->Compare(*b) < 0;
            });
    }
}//namespace hgl::ecs
