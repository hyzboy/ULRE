// TileMap2D
// 该示例演示2D TileMap渲染，使用TileData管理tile图片，通过uint16/32二维数组数据渲染tilemap

#include<hgl/type/StringList.h>
#include<hgl/graph/TileData.h>

#include"VulkanAppFramework.h"
#include<hgl/graph/VKTexture.h>
#include<hgl/graph/VKSampler.h>
#include<hgl/math/Math.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH = 1024;
constexpr uint32_t SCREEN_HEIGHT = 768;

constexpr uint TILE_SIZE = 64;          // 每个tile的像素大小
constexpr uint MAP_WIDTH = 16;          // tilemap宽度（tile数量）
constexpr uint MAP_HEIGHT = 12;         // tilemap高度（tile数量）

// TileMap数据结构
struct TileMapData
{
    uint32_t width, height;
    uint16_t *data;  // 二维数组数据，存储tile索引
    
    TileMapData(uint32_t w, uint32_t h) : width(w), height(h)
    {
        data = new uint16_t[w * h];
        memset(data, 0, sizeof(uint16_t) * w * h);
    }
    
    ~TileMapData()
    {
        delete[] data;
    }
    
    uint16_t GetTile(uint32_t x, uint32_t y) const
    {
        if (x >= width || y >= height) return 0;
        return data[y * width + x];
    }
    
    void SetTile(uint32_t x, uint32_t y, uint16_t tile_id)
    {
        if (x >= width || y >= height) return;
        data[y * width + x] = tile_id;
    }
};

// Tile贴图信息
struct TileBitmap
{
    uint8_t *data;
    uint32_t width, height;
    TileObject *to;
    
    TileBitmap() : data(nullptr), width(0), height(0), to(nullptr) {}
    ~TileBitmap() { delete[] data; }
};

class TestApp : public VulkanApplicationFramework
{
    ObjectList<TileBitmap> tile_list;
    TileData *tile_data = nullptr;
    TileMapData *tilemap = nullptr;
    
    float *position_data = nullptr;
    float *tex_coord_data = nullptr;
    uint16_t *indices_data = nullptr;
    
    VkFormat tile_texture_format = VK_FORMAT_R8G8B8A8_UNORM;
    
private:
    
    Sampler *sampler = nullptr;
    MaterialInstance *material_instance = nullptr;
    Primitive *primitive = nullptr;
    Renderable *render_obj = nullptr;
    Pipeline *pipeline = nullptr;
    
public:
    
    ~TestApp()
    {
        SAFE_DELETE_ARRAY(position_data);
        SAFE_DELETE_ARRAY(tex_coord_data);
        SAFE_DELETE_ARRAY(indices_data);
        SAFE_DELETE(tilemap);
        
        SAFE_DELETE(render_obj);
        SAFE_DELETE(primitive);
        SAFE_DELETE(sampler);
        SAFE_DELETE(material_instance);
        SAFE_DELETE(tile_data);
        
        for(auto *tb : tile_list)
        {
            delete tb;
        }
    }
    
    // 创建示例tile图片数据
    bool CreateSampleTiles()
    {
        // 创建4个不同颜色的tile作为示例
        const uint32_t colors[] = {
            0xFF00FF00,  // 绿色 - 草地
            0xFF8B4513,  // 棕色 - 土地
            0xFF0000FF,  // 蓝色 - 水
            0xFF808080   // 灰色 - 石头
        };
        
        for(int i = 0; i < 4; i++)
        {
            TileBitmap *tb = new TileBitmap;
            
            // 创建64x64的RGBA贴图
            tb->width = TILE_SIZE;
            tb->height = TILE_SIZE;
            tb->data = new uint8_t[TILE_SIZE * TILE_SIZE * 4];
            
            // 填充颜色
            uint32_t *pixels = (uint32_t*)tb->data;
            for(int j = 0; j < TILE_SIZE * TILE_SIZE; j++)
            {
                pixels[j] = colors[i];
            }
            
            tb->to = nullptr;
            tile_list.Add(tb);
        }
        
        return true;
    }
    
    // 创建示例tilemap数据
    bool CreateSampleTileMap()
    {
        tilemap = new TileMapData(MAP_WIDTH, MAP_HEIGHT);
        
        // 创建一个简单的示例地图
        for(uint32_t y = 0; y < MAP_HEIGHT; y++)
        {
            for(uint32_t x = 0; x < MAP_WIDTH; x++)
            {
                uint16_t tile_id = 0;
                
                // 创建一个简单的地形：边缘是水，中间有草地和土地
                if(x == 0 || x == MAP_WIDTH-1 || y == 0 || y == MAP_HEIGHT-1)
                {
                    tile_id = 2; // 水
                }
                else if((x + y) % 3 == 0)
                {
                    tile_id = 1; // 土地
                }
                else if((x * y) % 4 == 0)
                {
                    tile_id = 3; // 石头
                }
                else
                {
                    tile_id = 0; // 草地
                }
                
                tilemap->SetTile(x, y, tile_id);
            }
        }
        
        return true;
    }
    
    bool InitTileTexture()
    {
        tile_data = device->CreateTileData(tile_texture_format,
                                          TILE_SIZE, TILE_SIZE,
                                          tile_list.GetCount());
        
        if(!tile_data)
            return false;
        
        tile_data->BeginCommit();
        
        for(auto *tb : tile_list)
        {
            tb->to = tile_data->Commit(tb->data, tb->width * tb->height * 4, tb->width, tb->height);
        }
        
        tile_data->EndCommit();
        
        return true;
    }
    
    // 创建tilemap渲染几何体
    bool CreateTileMapGeometry()
    {
        const uint32_t total_tiles = MAP_WIDTH * MAP_HEIGHT;
        const uint32_t vertices_per_tile = 4;
        const uint32_t indices_per_tile = 6;
        
        const uint32_t total_vertices = total_tiles * vertices_per_tile;
        const uint32_t total_indices = total_tiles * indices_per_tile;
        
        position_data = new float[total_vertices * 2];    // x, y
        tex_coord_data = new float[total_vertices * 2];   // u, v
        indices_data = new uint16_t[total_indices];
        
        float *pos_ptr = position_data;
        float *tex_ptr = tex_coord_data;
        uint16_t *idx_ptr = indices_data;
        
        uint16_t vertex_index = 0;
        
        for(uint32_t y = 0; y < MAP_HEIGHT; y++)
        {
            for(uint32_t x = 0; x < MAP_WIDTH; x++)
            {
                uint16_t tile_id = tilemap->GetTile(x, y);
                
                // 计算tile在屏幕上的位置
                float left = (float)x * TILE_SIZE;
                float top = (float)y * TILE_SIZE;
                float right = left + TILE_SIZE;
                float bottom = top + TILE_SIZE;
                
                // 转换为NDC坐标 (-1 to 1)
                left = (left / SCREEN_WIDTH) * 2.0f - 1.0f;
                right = (right / SCREEN_WIDTH) * 2.0f - 1.0f;
                top = 1.0f - (top / SCREEN_HEIGHT) * 2.0f;
                bottom = 1.0f - (bottom / SCREEN_HEIGHT) * 2.0f;
                
                // 顶点位置 (左上、右上、右下、左下)
                pos_ptr[0] = left;   pos_ptr[1] = top;       // 左上
                pos_ptr[2] = right;  pos_ptr[3] = top;       // 右上
                pos_ptr[4] = right;  pos_ptr[5] = bottom;    // 右下
                pos_ptr[6] = left;   pos_ptr[7] = bottom;    // 左下
                pos_ptr += 8;
                
                // 纹理坐标
                if(tile_id < tile_list.GetCount())
                {
                    TileObject *to = tile_list[tile_id]->to;
                    const TileUVFloat &uv = to->uv_float;
                    
                    tex_ptr[0] = uv.left;   tex_ptr[1] = uv.top;      // 左上
                    tex_ptr[2] = uv.right;  tex_ptr[3] = uv.top;      // 右上
                    tex_ptr[4] = uv.right;  tex_ptr[5] = uv.bottom;   // 右下
                    tex_ptr[6] = uv.left;   tex_ptr[7] = uv.bottom;   // 左下
                }
                else
                {
                    // 默认UV坐标
                    tex_ptr[0] = 0.0f; tex_ptr[1] = 0.0f;
                    tex_ptr[2] = 1.0f; tex_ptr[3] = 0.0f;
                    tex_ptr[4] = 1.0f; tex_ptr[5] = 1.0f;
                    tex_ptr[6] = 0.0f; tex_ptr[7] = 1.0f;
                }
                tex_ptr += 8;
                
                // 索引 (两个三角形组成四边形)
                idx_ptr[0] = vertex_index + 0;
                idx_ptr[1] = vertex_index + 1;
                idx_ptr[2] = vertex_index + 2;
                idx_ptr[3] = vertex_index + 0;
                idx_ptr[4] = vertex_index + 2;
                idx_ptr[5] = vertex_index + 3;
                idx_ptr += 6;
                
                vertex_index += 4;
            }
        }
        
        return true;
    }
    
    bool InitMaterial()
    {
        material_instance = db->CreateMaterialInstance(OS_TEXT("res/material/VertexColor2D"));
        if(!material_instance)
            return false;
        
        MaterialParameters *mp_global = material_instance->GetMP(DescriptorSetsType::Global);
        if(mp_global)
        {
            mp_global->BindUBO("g_camera", GetViewportInfoBuffer());
            mp_global->Update();
        }
        
        MaterialParameters *mp_texture = material_instance->GetMP(DescriptorSetsType::Value);
        if(!mp_texture)
            return false;
        
        sampler = db->CreateSampler();
        if(!mp_texture->BindSampler("tex", tile_data->GetTexture(), sampler))
            return false;
        
        mp_texture->Update();
        
        pipeline = CreatePipeline(material_instance, InlinePipeline::Solid, Prim::Triangles);
        if(!pipeline)
            return false;
        
        return true;
    }
    
    bool InitGeometry()
    {
        const uint32_t total_tiles = MAP_WIDTH * MAP_HEIGHT;
        const uint32_t total_vertices = total_tiles * 4;
        const uint32_t total_indices = total_tiles * 6;
        
        AutoDelete<VAB> position_buffer = db->CreateVAB(VF_VEC2, total_vertices, position_data);
        AutoDelete<VAB> texcoord_buffer = db->CreateVAB(VF_VEC2, total_vertices, tex_coord_data);
        AutoDelete<IndexBuffer> index_buffer = db->CreateIBO16(total_indices, indices_data);
        
        render_obj = db->CreateRenderable(total_vertices);
        if(!render_obj)
            return false;
        
        render_obj->Set(VAN::Position, position_buffer);
        render_obj->Set(VAN::TexCoord, texcoord_buffer);
        render_obj->Set(index_buffer);
        
        return true;
    }
    
    bool Init()
    {
        if(!VulkanApplicationFramework::Init(SCREEN_WIDTH, SCREEN_HEIGHT))
            return false;
        
        if(!CreateSampleTiles())
            return false;
        
        if(!CreateSampleTileMap())
            return false;
        
        if(!InitTileTexture())
            return false;
        
        if(!CreateTileMapGeometry())
            return false;
        
        if(!InitMaterial())
            return false;
        
        if(!InitGeometry())
            return false;
        
        BuildCommandBuffer(render_obj);
        
        return true;
    }
    
    void BuildCommandBuffer(uint32_t index) override
    {
        VulkanApplicationFramework::BuildCommandBuffer(index, render_obj);
    }
    
    void Resize(int w, int h) override
    {
        VulkanApplicationFramework::Resize(w, h);
        VulkanApplicationFramework::BuildCommandBuffer(render_obj);
    }
};

int main(int, char **)
{
    TestApp app;
    
    if(!app.Init())
        return -1;
    
    return app.Run();
}