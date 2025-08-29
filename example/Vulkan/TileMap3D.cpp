// TileMap3D
// 该示例演示3D TileMap渲染，使用TileData管理tile图片，通过uint16/32二维数组数据在3D空间中渲染tilemap

#include<hgl/type/StringList.h>
#include<hgl/graph/TileData.h>

#include"VulkanAppFramework.h"
#include<hgl/graph/VKTexture.h>
#include<hgl/graph/VKSampler.h>
#include<hgl/graph/mtl/UBOCommon.h>
#include<hgl/math/Math.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH = 1024;
constexpr uint32_t SCREEN_HEIGHT = 768;

constexpr float TILE_SIZE = 1.0f;       // 每个tile的世界空间大小
constexpr uint MAP_WIDTH = 20;          // tilemap宽度（tile数量）
constexpr uint MAP_HEIGHT = 20;         // tilemap高度（tile数量）

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

class TestApp : public CameraAppFramework
{
    ObjectList<TileBitmap> tile_list;
    TileData *tile_data = nullptr;
    TileMapData *tilemap = nullptr;
    
    float *position_data = nullptr;
    float *tex_coord_data = nullptr;
    uint16_t *indices_data = nullptr;
    
    VkFormat tile_texture_format = VK_FORMAT_R8G8B8A8_UNORM;
    
private:
    
    SceneNode render_root;
    RenderList *render_list = nullptr;
    
    Sampler *sampler = nullptr;
    MaterialInstance *material_instance = nullptr;
    Primitive *primitive = nullptr;
    Renderable *render_obj = nullptr;
    Pipeline *pipeline = nullptr;
    
    DeviceBuffer *ubo_camera = nullptr;
    
public:
    
    ~TestApp()
    {
        SAFE_DELETE_ARRAY(position_data);
        SAFE_DELETE_ARRAY(tex_coord_data);
        SAFE_DELETE_ARRAY(indices_data);
        SAFE_DELETE(tilemap);
        
        SAFE_DELETE(render_list);
        SAFE_DELETE(render_obj);
        SAFE_DELETE(primitive);
        SAFE_DELETE(sampler);
        SAFE_DELETE(material_instance);
        SAFE_DELETE(sampler);
        SAFE_DELETE(tile_data);
        
        for(auto *tb : tile_list)
        {
            delete tb;
        }
    }
    
    // 创建示例tile图片数据
    bool CreateSampleTiles()
    {
        // 创建6个不同颜色和图案的tile作为示例
        const uint32_t colors[] = {
            0xFF228B22,  // 森林绿 - 草地
            0xFF8B4513,  // 鞍褐色 - 土地  
            0xFF4169E1,  // 皇家蓝 - 水
            0xFF708090,  // 石板灰 - 石头
            0xFF8FBC8F,  // 深海绿 - 森林
            0xFFDAA520   // 金麒麟色 - 沙地
        };
        
        for(int i = 0; i < 6; i++)
        {
            TileBitmap *tb = new TileBitmap;
            
            // 创建64x64的RGBA贴图
            tb->width = 64;
            tb->height = 64;
            tb->data = new uint8_t[64 * 64 * 4];
            
            uint32_t *pixels = (uint32_t*)tb->data;
            
            // 为不同tile创建不同的图案
            for(int y = 0; y < 64; y++)
            {
                for(int x = 0; x < 64; x++)
                {
                    uint32_t color = colors[i];
                    
                    // 添加一些纹理细节
                    if(i == 0) // 草地 - 添加一些随机点
                    {
                        if((x + y * 64) % 7 == 0)
                            color = 0xFF32CD32; // 酸橙绿
                    }
                    else if(i == 2) // 水 - 添加波纹效果
                    {
                        if((x + y) % 8 < 2)
                            color = 0xFF6495ED; // 矢车菊蓝
                    }
                    else if(i == 3) // 石头 - 添加裂纹
                    {
                        if(x % 16 == 0 || y % 16 == 0)
                            color = 0xFF2F4F4F; // 深石板灰
                    }
                    
                    pixels[y * 64 + x] = color;
                }
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
        
        // 创建一个更复杂的地形地图
        for(uint32_t y = 0; y < MAP_HEIGHT; y++)
        {
            for(uint32_t x = 0; x < MAP_WIDTH; x++)
            {
                uint16_t tile_id = 0;
                
                float center_x = MAP_WIDTH / 2.0f;
                float center_y = MAP_HEIGHT / 2.0f;
                float dist = sqrt((x - center_x) * (x - center_x) + (y - center_y) * (y - center_y));
                
                // 创建同心圆地形
                if(dist < 3)
                {
                    tile_id = 2; // 中心湖泊
                }
                else if(dist < 5)
                {
                    tile_id = 0; // 草地围绕湖泊
                }
                else if(dist < 8)
                {
                    if((x + y) % 3 == 0)
                        tile_id = 4; // 森林
                    else
                        tile_id = 1; // 土地
                }
                else if(dist < 10)
                {
                    if((x * y) % 5 == 0)
                        tile_id = 3; // 石头
                    else
                        tile_id = 5; // 沙地
                }
                else
                {
                    // 边缘地带
                    if(x == 0 || x == MAP_WIDTH-1 || y == 0 || y == MAP_HEIGHT-1)
                        tile_id = 2; // 边缘水域
                    else
                        tile_id = 5; // 沙漠
                }
                
                tilemap->SetTile(x, y, tile_id);
            }
        }
        
        return true;
    }
    
    bool InitTileTexture()
    {
        tile_data = device->CreateTileData(tile_texture_format,
                                          64, 64,
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
    
    // 创建tilemap渲染几何体（3D平面）
    bool CreateTileMapGeometry()
    {
        const uint32_t total_tiles = MAP_WIDTH * MAP_HEIGHT;
        const uint32_t vertices_per_tile = 4;
        const uint32_t indices_per_tile = 6;
        
        const uint32_t total_vertices = total_tiles * vertices_per_tile;
        const uint32_t total_indices = total_tiles * indices_per_tile;
        
        position_data = new float[total_vertices * 3];    // x, y, z
        tex_coord_data = new float[total_vertices * 2];   // u, v
        indices_data = new uint16_t[total_indices];
        
        float *pos_ptr = position_data;
        float *tex_ptr = tex_coord_data;
        uint16_t *idx_ptr = indices_data;
        
        uint16_t vertex_index = 0;
        
        // 将tilemap放置在XZ平面上，Y=0
        for(uint32_t z = 0; z < MAP_HEIGHT; z++)
        {
            for(uint32_t x = 0; x < MAP_WIDTH; x++)
            {
                uint16_t tile_id = tilemap->GetTile(x, z);
                
                // 计算tile在世界空间中的位置
                float left = (float)x * TILE_SIZE;
                float right = left + TILE_SIZE;
                float front = (float)z * TILE_SIZE;
                float back = front + TILE_SIZE;
                
                // 将地图居中
                float offset_x = -(MAP_WIDTH * TILE_SIZE) / 2.0f;
                float offset_z = -(MAP_HEIGHT * TILE_SIZE) / 2.0f;
                
                left += offset_x;
                right += offset_x;
                front += offset_z;
                back += offset_z;
                
                // 顶点位置 (四个角，Y=0平面)
                pos_ptr[0] = left;   pos_ptr[1] = 0.0f; pos_ptr[2] = front;   // 左前
                pos_ptr[3] = right;  pos_ptr[4] = 0.0f; pos_ptr[5] = front;   // 右前
                pos_ptr[6] = right;  pos_ptr[7] = 0.0f; pos_ptr[8] = back;    // 右后
                pos_ptr[9] = left;   pos_ptr[10] = 0.0f; pos_ptr[11] = back;  // 左后
                pos_ptr += 12;
                
                // 纹理坐标
                if(tile_id < tile_list.GetCount())
                {
                    TileObject *to = tile_list[tile_id]->to;
                    const TileUVFloat &uv = to->uv_float;
                    
                    tex_ptr[0] = uv.left;   tex_ptr[1] = uv.top;      // 左前
                    tex_ptr[2] = uv.right;  tex_ptr[3] = uv.top;      // 右前
                    tex_ptr[4] = uv.right;  tex_ptr[5] = uv.bottom;   // 右后
                    tex_ptr[6] = uv.left;   tex_ptr[7] = uv.bottom;   // 左后
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
        material_instance = db->CreateMaterialInstance(OS_TEXT("res/material/BasicTexture3D"));
        if(!material_instance)
            return false;
        
        MaterialParameters *mp_global = material_instance->GetMP(DescriptorSetsType::Global);
        if(mp_global)
        {
            mp_global->BindUBO("g_camera", GetCameraInfoBuffer());
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
        
        AutoDelete<VAB> position_buffer = db->CreateVAB(VF_VEC3, total_vertices, position_data);
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
    
    bool InitScene()
    {
        render_list = new RenderList(device);
        
        render_root.CreateSubNode(Matrix4f().identity(), render_obj);
        
        return true;
    }
    
    bool Init()
    {
        if(!CameraAppFramework::Init(SCREEN_WIDTH, SCREEN_HEIGHT))
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
        
        if(!InitScene())
            return false;
        
        VulkanApplicationFramework::BuildCommandBuffer(render_list);
        
        return true;
    }
    
    void BuildCommandBuffer(uint32_t index) override
    {
        render_root.RefreshMatrix();
        render_list->Expend(GetCameraInfo(), &render_root);
        
        VulkanApplicationFramework::BuildCommandBuffer(index, render_list);
    }
    
    void Resize(int w, int h) override
    {
        CameraAppFramework::Resize(w, h);
        VulkanApplicationFramework::BuildCommandBuffer(render_list);
    }
};

int main(int, char **)
{
    TestApp app;
    
    if(!app.Init())
        return -1;
    
    return app.Run();
}