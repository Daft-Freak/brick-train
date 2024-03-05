#pragma once

#include <SDL.h>

#include <filesystem>
#include <string>
#include <vector>

class World final
{
public:
    ~World();

    bool loadSave(const std::filesystem::path &path);

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

        std::vector<Minifig> minifigs;
    };

    uint16_t width = 0;
    uint16_t height = 0;

    uint8_t *tileObjectType = nullptr;

    std::vector<Object> objects;
};