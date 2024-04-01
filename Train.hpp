#pragma once

#include "Object.hpp"

class World;

class Train final
{
public:
    Train(World &world, uint16_t engineId, std::string name);
    Train(Train &) = delete;
    Train(Train &&other);

    void update(uint32_t deltaMs);

    void render(SDL_Renderer *renderer, int scrollX, int scrollY, float zoom);

    void placeInObject(Object &obj);

private:

    struct Part
    {
        Part(Train &parent, Object &&object);

        bool update(uint32_t deltaMs, int speed);

        void placeInObject(Object &obj);
    
        void getWorldCoord(const std::tuple<int, int> &coord, int &x, int &y, const Object &obj);

        void copyPosition(const Part &other);

        Train &parent;
        Object object;

        float objectCoordPos = 0.0f;
        bool objectCoordReverse = false; // moving along the coords backwards
        bool objectAltCoords = false; // use the other coords (points)
        int curObjectX = 0, curObjectY = 0;

        bool prevObjectCoordReverse[2] = {false, false};
        bool prevObjectAltCoords[2] = {false, false};
        int prevObjectX[2] = {0, 0}, prevObjectY[2] = {0, 0};
    };

    void enterObject(Object &obj);
    void leaveObject(Object &obj);

    World &world;
    Part engine;

    int speed;
};