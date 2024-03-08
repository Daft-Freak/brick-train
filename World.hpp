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

    bool loadSave(const std::filesystem::path &path, SDL_Renderer *renderer);

    void render(SDL_Renderer *renderer);

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
    
        uint16_t id;
        uint16_t x, y;
        std::string name;

        std::shared_ptr<SDL_Texture> texture;
        const ObjectData *data;

        std::vector<Minifig> minifigs;
    };

    TextureLoader &texLoader;
    ObjectDataStore &objectDataStore;

    uint16_t width = 0;
    uint16_t height = 0;

    uint8_t *tileObjectType = nullptr;

    std::shared_ptr<SDL_Texture> backdrop;

    std::vector<Object> objects;
};