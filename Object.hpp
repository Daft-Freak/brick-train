#pragma once

#include <SDL.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "ObjectData.hpp"

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

    uint16_t getId() const;

    int getX() const;
    int getY() const;

    void setX(int x);
    void setY(int y);
    void setPosition(int x, int y);

    const ObjectData *getData() const;

    void replace(uint16_t newId, std::shared_ptr<SDL_Texture> newTex = nullptr, const ObjectData *newData = nullptr);

    void addMinifig(Minifig &&minifig);

    const ObjectData::Frameset *getCurrentFrameset() const;
    int getFrameDelay() const;

    void setDefaultAnimation();
    bool setAnimation(int index);
    bool setAnimation(std::string_view name);

    void setAnimationFrame(int frame);

    std::tuple<int, int> getFrameSize() const;

    void setPixelPos(float x, float y);

    void setTargetPos(int tx, int ty, int vx, int vy, bool reverse = false);

private:
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