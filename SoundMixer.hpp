#pragma once

#include <map>

#include <SDL_mixer.h>

#include "SoundLoader.hpp"

class SoundMixer final
{
public:
    SoundMixer(FileLoader &fileLoader);

    int playSound(const std::shared_ptr<Mix_Chunk> &sound, int priority);

    SoundLoader &getLoader();

private:
    SoundLoader loader;

    std::map<int, std::shared_ptr<Mix_Chunk>> playingSounds;
};