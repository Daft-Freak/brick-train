#pragma once

#include "Object.hpp"

class World;

class Train final
{
public:
    Train(World &world, uint16_t engineId, std::string name);

    void update(uint32_t deltaMs);

    void render(SDL_Renderer *renderer, int scrollX, int scrollY, float zoom);
private:

    World &world;
    Object engine;
};