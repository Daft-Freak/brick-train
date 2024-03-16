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
    World(FileLoader &fileLoader, TextureLoader &texLoader, ObjectDataStore &objectDataStore);
    ~World();

    bool loadSave(const std::filesystem::path &path);

    void update(uint32_t deltaMs);

    void handleEvent(SDL_Event &event);

    void render(SDL_Renderer *renderer);

    void setWindowSize(unsigned int windowWidth, unsigned int windowHeight);

private:

    enum class ObjectMotion
    {
        None,

        // used by TimeEvents
        // the comments in ee.ini claim there are other (unused) values
        // (World, Random, Centered)
        Port, // right -> left
        Starboard, // left -> right

        // there's also something that makes the space shuttle go up
    };

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

        void setDefaultAnimation();
        void setAnimation(int index);

        std::tuple<int, int> getFrameSize() const;

        void setTargetPos(int tx, int ty, int vx, int vy, bool reverse = false);

        uint16_t id;
        int x, y;
        std::string name;

        std::shared_ptr<SDL_Texture> texture;
        const ObjectData *data;

        std::vector<Minifig> minifigs;

        int currentAnimation = -1, nextAnimation = -1;
        int currentAnimationFrame = 0;
        int animationTimer = 0;

        // "screen" aligned objects
        float pixelX = 0.0f, pixelY = 0.0f;

        // moving objects
        int targetX = 0, targetY = 0;
        int velX = 0, velY = 0;

        bool reverse = false;
    };

    struct TimeEvent
    {
        int startDay, startMonth, endDay, endMonth;
        int startHour, startMin, endHour, endMin;
        int resId, resFrameset;
        int periodMax;
        ObjectMotion type;
        int x, y;
    };

    struct LoadEvent
    {
        int startDay, startMonth;
        int endDay, endMonth;

        int oldId, newId;
    };

    void loadEasterEggs();

    void clampScroll();

    Object &addObject(uint16_t id, uint16_t x, uint16_t y, std::string name);
    Object *getObjectAt(unsigned int x, unsigned int y);

    void applyInsertEasterEggs();
    void applyLoadEasterEggs();

    static const int tileSize = 16;

    FileLoader &fileLoader;
    TextureLoader &texLoader;
    ObjectDataStore &objectDataStore;

    std::vector<TimeEvent> timeEvents;
    std::vector<LoadEvent> loadEvents;

    unsigned int windowWidth = 0, windowHeight = 0;
    int scrollX = 0, scrollY = 0;
    float zoom = 1.0f;

    uint16_t width = 0;
    uint16_t height = 0;

    uint8_t *tileObjectType = nullptr;

    std::string backdropPath;
    std::shared_ptr<SDL_Texture> backdrop;

    std::vector<Object> objects;
};