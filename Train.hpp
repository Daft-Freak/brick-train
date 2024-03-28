#pragma once

#include "Object.hpp"

class World;

class Train final
{
public:
    Train(World &world, uint16_t engineId, std::string name);

    void update(uint32_t deltaMs);

    void render(SDL_Renderer *renderer, int scrollX, int scrollY, float zoom);

    void placeInObject(Object &obj);
private:

    void getPixelCoord(const std::tuple<int, int> &coord, int &x, int &y, const Object &obj);

    World &world;
    Object engine;

    int speed;

    float objectCoordPos = 0.0f;
    bool objectCoordReverse = false; // moving along the coords backwards
    bool objectAltCoords = false; // use the other coords (points)
    int curObjectX = 0, curObjectY = 0;

    bool prevObjectCoordReverse = false;
    bool prevObjectAltCoords = false;
    int prevObjectX = 0, prevObjectY = 0;
};