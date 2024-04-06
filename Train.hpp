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
        bool update(uint32_t deltaMs, Part &prevPart);

        void placeInObject(Object &obj);
    
        void getWorldCoord(const std::tuple<int, int> &coord, int &x, int &y, const Object &obj);

        void copyPosition(const Part &other);

        std::tuple<float, float> getNextCarriagePos(int &finalCoordIndex, float &finalCoordPos);

        Object &getObject();

        bool getValidPos() const;
        bool getOffscreen() const;

        bool isInTunnel() const;

    private:
        struct CoordMeta
        {
            bool reverse = false; // moving along the coords backwards
            bool alternate = false; // use the other coords (points)
            int x = 0, y = 0;
        };

        void setPosition(Object *obj, const ObjectData *objData, float newX, float newY);

        bool enterNextObject(Object *&obj, const ObjectData *&objData);

        CoordMeta *getCoordMeta(int index);

        std::tuple<float, float> lookBehind(int dist, const Object *obj, const ObjectData *objData, int &lastUsedObj, int &finalObjectIndex, float &finalCoordPos);

        Train &parent;
        Object object;

        bool validPos = false;
        bool offscreen = false; // technically more like "has left the world"

        float objectCoordPos = 0.0f;
        CoordMeta curObjectCoord;

        CoordMeta prevObjectCoord[3];
    };

    void enterObject(Part &part, Object &obj);
    void leaveObject(Part &part, Object &obj);

    World &world;
    Part engine;

    std::vector<Part> carriages;

    int speed;
};