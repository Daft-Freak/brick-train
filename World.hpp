#pragma once

#include <SDL.h>

#include <filesystem>
#include <random>
#include <string>
#include <vector>

#include "Object.hpp"
#include "ObjectDataStore.hpp"
#include "TextureLoader.hpp"
#include "Train.hpp"

class World final
{
public:
    World(FileLoader &fileLoader, TextureLoader &texLoader, ObjectDataStore &objectDataStore);
    ~World();

    bool loadSave(const std::filesystem::path &path);

    void update(uint32_t deltaMs, SoundMixer &sound);

    void handleEvent(SDL_Event &event);

    void render(SDL_Renderer *renderer);

    void setWindowSize(unsigned int windowWidth, unsigned int windowHeight);

    ObjectDataStore &getObjectDataStore();

    Object createObject(uint16_t id, uint16_t x, uint16_t y, std::string name);
    Object &addObject(uint16_t id, uint16_t x, uint16_t y, std::string name);
    Object *getObjectAt(unsigned int x, unsigned int y);

    std::vector<Object *> getTunnels(bool shuffled = false);

    static const int tileSize = 16;

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

    struct TimeEvent
    {
        int startDay, startMonth, endDay, endMonth;
        int startHour, startMin, endHour, endMin;
        int resId, resFrameset;
        int periodMax;
        ObjectMotion type;
        int x, y;

        int periodTimer;
    };

    struct LoadEvent
    {
        int startDay, startMonth;
        int endDay, endMonth;

        int oldId, newId;
    };

    void loadEasterEggs();

    void clampScroll();

    void applyInsertEasterEggs();
    void applyLoadEasterEggs();
    void updateTimeEasterEggs(uint32_t deltaMs);

    FileLoader &fileLoader;
    TextureLoader &texLoader;
    ObjectDataStore &objectDataStore;

    std::mt19937 randomGen;

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

    std::vector<Train> trains;
};