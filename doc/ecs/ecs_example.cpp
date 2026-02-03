// ECS example: EntityManager + Mask + Factory + Template
// This file is a self-contained example for documentation.

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <algorithm>
#include <iostream>

// -----------------------------------------------------------------------------
// Minimal Bit Operations (standalone demo, matches idea in BitOperations.h)
// -----------------------------------------------------------------------------
namespace hgl
{
    template<typename T>
    constexpr bool is_bit_set(const T value, int offset) noexcept
    {
        return value & (T(1) << offset);
    }

    template<typename T>
    constexpr void set_bit(T &value, int offset) noexcept
    {
        value |= (T(1) << offset);
    }

    template<typename T>
    constexpr void clear_bit(T &value, int offset) noexcept
    {
        value &= ~(T(1) << offset);
    }
}

// -----------------------------------------------------------------------------
// Simple math types
// -----------------------------------------------------------------------------
struct Vec2
{
    float x = 0.0f;
    float y = 0.0f;

    Vec2 operator+(const Vec2 &o) const { return {x + o.x, y + o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
};

// -----------------------------------------------------------------------------
// Components
// -----------------------------------------------------------------------------
struct Transform
{
    Vec2 position;
    float rotation = 0.0f;
    Vec2 scale{1.0f, 1.0f};
};

struct Sprite
{
    std::string texture_path;
    Vec2 size;
};

struct Color
{
    float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
};

struct Layer
{
    int z_order = 0;
};

struct Velocity
{
    Vec2 velocity;
};

struct Collider
{
    float width = 0.0f;
    float height = 0.0f;
    bool is_trigger = false;
};

// -----------------------------------------------------------------------------
// Component type mask
// -----------------------------------------------------------------------------
enum class ComponentType : uint32_t
{
    Transform = 0,
    Sprite = 1,
    Color = 2,
    Layer = 3,
    Velocity = 4,
    Collider = 5
};

// -----------------------------------------------------------------------------
// EntityManager: central storage + mask query
// -----------------------------------------------------------------------------
class EntityManager
{
private:
    std::map<uint32_t, uint32_t> entity_masks;
    std::map<uint32_t, Transform> transforms;
    std::map<uint32_t, Sprite> sprites;
    std::map<uint32_t, Color> colors;
    std::map<uint32_t, Layer> layers;
    std::map<uint32_t, Velocity> velocities;
    std::map<uint32_t, Collider> colliders;
    uint32_t next_entity_id = 1;

public:
    uint32_t create_entity()
    {
        uint32_t id = next_entity_id++;
        entity_masks[id] = 0;
        return id;
    }

    void destroy_entity(uint32_t entity_id)
    {
        entity_masks.erase(entity_id);
        transforms.erase(entity_id);
        sprites.erase(entity_id);
        colors.erase(entity_id);
        layers.erase(entity_id);
        velocities.erase(entity_id);
        colliders.erase(entity_id);
    }

    // Add components
    void add_transform(uint32_t entity_id, const Transform &tf)
    {
        transforms[entity_id] = tf;
        hgl::set_bit(entity_masks[entity_id], static_cast<int>(ComponentType::Transform));
    }

    void add_sprite(uint32_t entity_id, const Sprite &sprite)
    {
        sprites[entity_id] = sprite;
        hgl::set_bit(entity_masks[entity_id], static_cast<int>(ComponentType::Sprite));
    }

    void add_color(uint32_t entity_id, const Color &color)
    {
        colors[entity_id] = color;
        hgl::set_bit(entity_masks[entity_id], static_cast<int>(ComponentType::Color));
    }

    void add_layer(uint32_t entity_id, const Layer &layer)
    {
        layers[entity_id] = layer;
        hgl::set_bit(entity_masks[entity_id], static_cast<int>(ComponentType::Layer));
    }

    void add_velocity(uint32_t entity_id, const Velocity &vel)
    {
        velocities[entity_id] = vel;
        hgl::set_bit(entity_masks[entity_id], static_cast<int>(ComponentType::Velocity));
    }

    void add_collider(uint32_t entity_id, const Collider &col)
    {
        colliders[entity_id] = col;
        hgl::set_bit(entity_masks[entity_id], static_cast<int>(ComponentType::Collider));
    }

    // Get components
    Transform *get_transform(uint32_t entity_id)
    {
        if (!has_component(entity_id, ComponentType::Transform)) return nullptr;
        return &transforms[entity_id];
    }

    Sprite *get_sprite(uint32_t entity_id)
    {
        if (!has_component(entity_id, ComponentType::Sprite)) return nullptr;
        return &sprites[entity_id];
    }

    Color *get_color(uint32_t entity_id)
    {
        if (!has_component(entity_id, ComponentType::Color)) return nullptr;
        return &colors[entity_id];
    }

    Layer *get_layer(uint32_t entity_id)
    {
        if (!has_component(entity_id, ComponentType::Layer)) return nullptr;
        return &layers[entity_id];
    }

    Velocity *get_velocity(uint32_t entity_id)
    {
        if (!has_component(entity_id, ComponentType::Velocity)) return nullptr;
        return &velocities[entity_id];
    }

    Collider *get_collider(uint32_t entity_id)
    {
        if (!has_component(entity_id, ComponentType::Collider)) return nullptr;
        return &colliders[entity_id];
    }

    bool has_component(uint32_t entity_id, ComponentType type) const
    {
        auto it = entity_masks.find(entity_id);
        if (it == entity_masks.end()) return false;
        return hgl::is_bit_set(it->second, static_cast<int>(type));
    }

    static constexpr uint32_t make_mask(std::initializer_list<ComponentType> types)
    {
        uint32_t mask = 0;
        for (auto type : types)
        {
            mask |= (1u << static_cast<uint32_t>(type));
        }
        return mask;
    }

    std::vector<uint32_t> get_entities_with_components(uint32_t required_mask) const
    {
        std::vector<uint32_t> result;
        for (const auto &[entity_id, mask] : entity_masks)
        {
            if ((mask & required_mask) == required_mask)
            {
                result.push_back(entity_id);
            }
        }
        return result;
    }
};

// -----------------------------------------------------------------------------
// Systems: only logic
// -----------------------------------------------------------------------------
class PhysicsSystem
{
private:
    EntityManager *entity_mgr;
    float delta_time = 0.016f;

public:
    explicit PhysicsSystem(EntityManager *mgr) : entity_mgr(mgr) {}

    void update()
    {
        const uint32_t required = EntityManager::make_mask({ComponentType::Transform, ComponentType::Velocity});
        auto entities = entity_mgr->get_entities_with_components(required);

        for (auto id : entities)
        {
            auto *tf = entity_mgr->get_transform(id);
            auto *vel = entity_mgr->get_velocity(id);
            tf->position = tf->position + vel->velocity * delta_time;
        }
    }
};

class RenderSystem
{
private:
    EntityManager *entity_mgr;

public:
    explicit RenderSystem(EntityManager *mgr) : entity_mgr(mgr) {}

    void render()
    {
        const uint32_t required = EntityManager::make_mask({ComponentType::Transform, ComponentType::Sprite});
        auto entities = entity_mgr->get_entities_with_components(required);

        std::sort(entities.begin(), entities.end(), [this](uint32_t a, uint32_t b) {
            Layer *la = entity_mgr->get_layer(a);
            Layer *lb = entity_mgr->get_layer(b);
            int za = la ? la->z_order : 0;
            int zb = lb ? lb->z_order : 0;
            return za > zb;
        });

        for (auto id : entities)
        {
            auto *tf = entity_mgr->get_transform(id);
            auto *sprite = entity_mgr->get_sprite(id);
            auto *color = entity_mgr->get_color(id);

            std::cout << "Render Entity " << id
                      << " at (" << tf->position.x << ", " << tf->position.y << ")"
                      << " tex=" << sprite->texture_path
                      << " color=" << color->a << "\n";
        }
    }
};

// -----------------------------------------------------------------------------
// Factory
// -----------------------------------------------------------------------------
class EntityFactory
{
private:
    EntityManager *entity_mgr;

public:
    explicit EntityFactory(EntityManager *mgr) : entity_mgr(mgr) {}

    uint32_t create_player(const Vec2 &position)
    {
        uint32_t entity = entity_mgr->create_entity();
        entity_mgr->add_transform(entity, {position, 0.0f, {1.0f, 1.0f}});
        entity_mgr->add_sprite(entity, {"assets/player.png", {64.0f, 64.0f}});
        entity_mgr->add_color(entity, {1.0f, 1.0f, 1.0f, 1.0f});
        entity_mgr->add_layer(entity, {10});
        entity_mgr->add_velocity(entity, {{0.0f, 0.0f}});
        entity_mgr->add_collider(entity, {64.0f, 64.0f, false});
        return entity;
    }

    uint32_t create_background(const Vec2 &position, const std::string &sprite_path)
    {
        uint32_t entity = entity_mgr->create_entity();
        entity_mgr->add_transform(entity, {position, 0.0f, {2.0f, 2.0f}});
        entity_mgr->add_sprite(entity, {sprite_path, {800.0f, 600.0f}});
        entity_mgr->add_color(entity, {1.0f, 1.0f, 1.0f, 0.8f});
        entity_mgr->add_layer(entity, {0});
        return entity;
    }
};

// -----------------------------------------------------------------------------
// Template
// -----------------------------------------------------------------------------
struct EntityTemplate
{
    std::string name;
    std::optional<Transform> transform;
    std::optional<Sprite> sprite;
    std::optional<Color> color;
    std::optional<Layer> layer;
    std::optional<Velocity> velocity;
    std::optional<Collider> collider;

    uint32_t instantiate(EntityManager *mgr) const
    {
        uint32_t entity = mgr->create_entity();
        if (transform) mgr->add_transform(entity, *transform);
        if (sprite) mgr->add_sprite(entity, *sprite);
        if (color) mgr->add_color(entity, *color);
        if (layer) mgr->add_layer(entity, *layer);
        if (velocity) mgr->add_velocity(entity, *velocity);
        if (collider) mgr->add_collider(entity, *collider);
        return entity;
    }

    uint32_t instantiate_at(EntityManager *mgr, const Vec2 &position) const
    {
        EntityTemplate copy = *this;
        if (copy.transform) copy.transform->position = position;
        return copy.instantiate(mgr);
    }
};

namespace templates
{
    inline EntityTemplate player()
    {
        EntityTemplate tmpl;
        tmpl.name = "Player";
        tmpl.transform = Transform{{0, 0}, 0.0f, {1.0f, 1.0f}};
        tmpl.sprite = Sprite{"assets/player.png", {64, 64}};
        tmpl.color = Color{1.0f, 1.0f, 1.0f, 1.0f};
        tmpl.layer = Layer{10};
        tmpl.velocity = Velocity{{0.0f, 0.0f}};
        tmpl.collider = Collider{64.0f, 64.0f, false};
        return tmpl;
    }
}

// -----------------------------------------------------------------------------
// Main demo
// -----------------------------------------------------------------------------
int main()
{
    EntityManager entity_mgr;
    EntityFactory factory(&entity_mgr);

    PhysicsSystem physics_sys(&entity_mgr);
    RenderSystem render_sys(&entity_mgr);

    // Factory: one-line creation
    uint32_t player = factory.create_player({100.0f, 200.0f});
    uint32_t bg = factory.create_background({400.0f, 300.0f}, "assets/bg.png");

    // Template: data-driven creation
    uint32_t player2 = templates::player().instantiate_at(&entity_mgr, {150.0f, 250.0f});

    // Update & Render
    physics_sys.update();
    render_sys.render();

    std::cout << "Entities: player=" << player << ", bg=" << bg << ", player2=" << player2 << "\n";
    return 0;
}
