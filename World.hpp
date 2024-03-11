#pragma once

#include <SDL.h>

#include <filesystem>
#include <string>
#include <vector>

#include "ObjectDataStore.hpp"
#include "TextureLoader.hpp"

class World final
{
public:
    World(TextureLoader &texLoader, ObjectDataStore &objectDataStore);
    ~World();

    bool loadSave(const std::filesystem::path &path);

    void update(uint32_t deltaMs);

    void handleEvent(SDL_Event &event);

    void render(SDL_Renderer *renderer);

    void setWindowSize(unsigned int windowWidth, unsigned int windowHeight);

private:
    struct Minifig
    {
        uint32_t id;
        std::string name;
    };

    class Object
    {
    public:
        Object(uint16_t id, uint16_t x, uint16_t y, std::string name, std::shared_ptr<SDL_Texture> texture, const ObjectData *data);

        void update(uint32_t deltaMs);

        void render(SDL_Renderer *renderer, int scrollX, int scrollY, int z, float zoom);
        void renderDebug(SDL_Renderer *renderer, int scrollX, int scrollY, float zoom);

        const ObjectData::Frameset *getCurrentFrameset() const;
        int getFrameDelay() const;

        void setAnimation(int index);

        uint16_t id;
        uint16_t x, y;
        std::string name;

        std::shared_ptr<SDL_Texture> texture;
        const ObjectData *data;

        std::vector<Minifig> minifigs;

        int currentAnimation = -1, nextAnimation = -1;
        int currentAnimationFrame = 0;
        int animationTimer = 0;
    };

    void clampScroll();

    Object &addObject(uint16_t id, uint16_t x, uint16_t y, std::string name);

    static const int tileSize = 16;

    TextureLoader &texLoader;
    ObjectDataStore &objectDataStore;

    unsigned int windowWidth = 0, windowHeight = 0;
    int scrollX = 0, scrollY = 0;
    float zoom = 1.0f;

    uint16_t width = 0;
    uint16_t height = 0;

    uint8_t *tileObjectType = nullptr;

    std::shared_ptr<SDL_Texture> backdrop;

    std::vector<Object> objects;
};