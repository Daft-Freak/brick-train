#include "Train.hpp"

#include "World.hpp"

Train::Train(World &world, uint16_t engineId, std::string name) : world(world), engine(std::move(world.addObject(engineId, 0, 0, name)))
{
}

void Train::update(uint32_t deltaMs)
{
    static int tf = 0;
    engine.update(deltaMs);

    tf = (tf + 1) % 128;

    engine.setAnimationFrame(tf);
}

void Train::render(SDL_Renderer *renderer, int scrollX, int scrollY, float zoom)
{
    engine.render(renderer, scrollX, scrollY, 6, zoom);
}
