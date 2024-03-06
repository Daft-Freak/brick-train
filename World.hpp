#pragma once

#include <SDL.h>

#include <filesystem>
#include <string>
#include <vector>

#include "TextureLoader.hpp"

class World final
{
public:
    World(TextureLoader &texLoader);
    ~World();

    bool loadSave(const std::filesystem::path &path, SDL_Renderer *renderer);

    void render(SDL_Renderer *renderer);

private:
    struct Minifig
    {
        uint32_t id;
        std::string name;
    };

    struct Object
    {
        uint16_t id;
        uint16_t x, y;
        std::string name;

        std::shared_ptr<SDL_Texture> texture;

        std::vector<Minifig> minifigs;
    };

    TextureLoader &texLoader;

    uint16_t width = 0;
    uint16_t height = 0;

    uint8_t *tileObjectType = nullptr;

    std::shared_ptr<SDL_Texture> backdrop;

    std::vector<Object> objects;
};